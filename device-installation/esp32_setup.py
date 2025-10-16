#!/usr/bin/env python3
"""
Auto-setup a consistent device handle for ESP32 across Linux/macOS/Windows.

- Linux:  creates a udev rule -> /dev/esp32-led, reloads udev, exports ESP32_PORT
- macOS:  creates /usr/local/dev/esp32-led -> /dev/cu.* symlink, exports ESP32_PORT
- Windows: resolves \\.\COMx and sets user env ESP32_PORT via `setx`

No VID/PID/serial arguments required. We infer from current devices.
Note: you have to reopen your shell 
"""

import os, sys, platform, shlex, subprocess, re
from pathlib import Path

NAME = "esp32-led"  # desired friendly handle
ENV_KEY = "ESP32_PORT"

def run(cmd, shell=False, check=True):
    print(f"$ {cmd if isinstance(cmd, str) else ' '.join(map(shlex.quote, cmd))}")
    r = subprocess.run(cmd, shell=shell, text=True,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if r.stdout: print(r.stdout.strip())
    if r.stderr: print(r.stderr.strip(), file=sys.stderr)
    if check and r.returncode != 0: sys.exit(r.returncode)
    return r

# ---------- Linux ----------
def linux_find_current_device():
    # Prefer stable by-id entries
    byid = Path("/dev/serial/by-id")
    if byid.exists():
        # Common ESP32 bridges / native CDC name hints:
        hints = ("Silicon_Labs", "CP210", "usbserial", "wchusbserial", "Espressif", "USB_to_UART", "usbmodem")
        cands = sorted(p for p in byid.iterdir() if any(h in p.name for h in hints))
        if cands:
            # Resolve to real device node (e.g., /dev/ttyUSB0 or /dev/ttyACM0)
            return str(cands[0].resolve())
    # Fallback: if only one ttyUSB/ttyACM is present, pick it
    tty = [*Path("/dev").glob("ttyUSB*"), *Path("/dev").glob("ttyACM*")]
    if len(tty) == 1:
        return str(tty[0])
    print("Linux: Could not uniquely identify ESP32 device. Plug only one board or use /dev/serial/by-id.", file=sys.stderr)
    sys.exit(1)

def linux_install_udev(realnode):
    if os.geteuid() != 0:
        os.execvp("sudo", ["sudo", sys.executable] + sys.argv)

    # Read normalized udev ENV properties from the actual node
    out = run(["udevadm", "info", "-n", realnode]).stdout
    def grab(key):
        m = re.search(rf"^E:\s*{re.escape(key)}=(.+)$", out, re.M)
        return m.group(1) if m else None
    vid = (grab("ID_VENDOR_ID") or "").lower()
    pid = (grab("ID_MODEL_ID") or "").lower()
    serial = grab("ID_SERIAL_SHORT")

    # Build two rules: cover ttyUSB* (UART bridges) and ttyACM* (native CDC)
    envs = [f'ENV{{ID_VENDOR_ID}}=="{vid}"'] if vid else []
    if pid: envs.append(f'ENV{{ID_MODEL_ID}}=="{pid}"')
    if serial: envs.append(f'ENV{{ID_SERIAL_SHORT}}=="{serial}"')
    else: envs.append('ENV{ID_SERIAL_SHORT}!=""')

    action = f'SYMLINK+="{NAME}", GROUP="dialout", MODE="0660", TAG+="uaccess"'
    lines = []
    for kpat in ("ttyUSB*", "ttyACM*"):
        parts = [ 'SUBSYSTEM=="tty"', f'KERNEL=="{kpat}"' ] + envs + [ action ]
        lines.append(", ".join(parts))

    rule_path = Path("/etc/udev/rules.d/99-esp32.rules")
    rule_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    run(["udevadm", "control", "--reload-rules"])
    run(["udevadm", "trigger", "--subsystem-match=tty"], check=False)

    # Export ENV for shells (user-level); also write a simple config file
    export_path = Path("/etc/profile.d/esp32_port.sh")
    export_path.write_text(f'export {ENV_KEY}=/dev/{NAME}\n', encoding="utf-8")
    Path("/etc/esp32-port").write_text(f"/dev/{NAME}\n", encoding="utf-8")

    print(f"Linux: udev rule installed at {rule_path}")
    print(f"Linux: {ENV_KEY}=/dev/{NAME} (in /etc/profile.d/esp32_port.sh)")
    print(f"Tip: unplug/replug if /dev/{NAME} not visible yet.")

# ---------- macOS ----------
def macos_find_current_device():
    r = run("ls /dev/cu.*", shell=True, check=False).stdout
    cands = [p for p in r.split() if p.startswith("/dev/cu.")]
    hints = ("SLAB_USBtoUART","usbserial","wchusbserial","usbmodem","Espressif")
    cands = [c for c in cands if any(h in c for h in hints)] or cands
    if not cands:
        print("macOS: no /dev/cu.* candidate found. Plug the ESP32.", file=sys.stderr)
        sys.exit(1)
    return cands[0]

def macos_install_alias(realnode):
    dest = Path("/usr/local/dev") / NAME
    dest.parent.mkdir(parents=True, exist_ok=True)
    cmd = f"ln -sfn {shlex.quote(realnode)} {shlex.quote(str(dest))}"
    if os.geteuid() != 0:
        run(f"sudo {cmd}", shell=True)
    else:
        run(cmd, shell=True)

    etc = Path("/usr/local/etc"); etc.mkdir(parents=True, exist_ok=True)
    (etc / "esp32-port").write_text(str(dest) + "\n", encoding="utf-8")

    # Add ENV to user shells (~/.zshrc and ~/.bashrc) idempotently
    export_line = f'export {ENV_KEY}="{dest}"'
    for rc in (Path.home()/".zshrc", Path.home()/".bashrc"):
        try:
            text = rc.read_text(encoding="utf-8") if rc.exists() else ""
            if export_line not in text:
                rc.write_text(text + ("\n" if text and not text.endswith("\n") else "") + export_line + "\n", encoding="utf-8")
        except Exception as e:
            print(f"Note: couldn’t update {rc}: {e}", file=sys.stderr)

    print(f"macOS: {dest} -> {realnode}")
    print(f"macOS: {ENV_KEY}={dest} added to ~/.zshrc and ~/.bashrc")

# ---------- Windows ----------
def windows_find_and_set_env():
    # Prefer common USB-UART vendors without requiring VID/PID args
    ps = r"""
$ports = Get-CimInstance Win32_PnPEntity | Where-Object {
  $_.Name -match 'COM\d+' -and (
    $_.Manufacturer -match 'Silicon Labs|FTDI|wch|WCH|Espressif' -or
    $_.PNPDeviceID -match 'VID_10C4|VID_0403|VID_1A86|VID_303A'
  )
}
if (-not $ports) {
  # Fallback: any COM device
  $ports = Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -match 'COM\d+' }
}
if (-not $ports) { Write-Error 'No serial (COM) devices found.'; exit 1 }

$com = ($ports | Select-Object -First 1).Name -replace '.*\((COM\d+)\).*','$1'
$canon = "\\.\$com"
# Persist user env
setx ESP32_PORT $canon > $null
# Also write a well-known file for tools
$cfg = 'C:\ProgramData\esp32-port'
Set-Content -Path $cfg -Value $canon -Encoding ascii
Write-Output $canon
"""
    r = run(["powershell","-NoProfile","-ExecutionPolicy","Bypass","-Command", ps])
    print("Windows:", r.stdout.strip().splitlines()[-1])
    print("Windows: set user env ESP32_PORT (re-open terminal to see it)")

# ---------- main ----------
def main():
    osname = platform.system().lower()
    if osname == "linux":
        node = linux_find_current_device()
        linux_install_udev(node)
        print(f"Linux: Done. Use $ESP32_PORT or /dev/{NAME}")
    elif osname == "darwin":
        node = macos_find_current_device()
        macos_install_alias(node)
        print(f"macOS: Done. Use $ESP32_PORT (points to /usr/local/dev/{NAME})")
    elif osname == "windows":
        windows_find_and_set_env()
        print(r"Windows: Done. Use %ESP32_PORT% (e.g., \\.\COM5)")
    else:
        print(f"Unsupported OS: {osname}", file=sys.stderr); sys.exit(2)

if __name__ == "__main__":
    main()


