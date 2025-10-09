#!/usr/bin/env bash
set -euo pipefail

RULE_SRC="./99-esp32.rules"
RULE_DST="/etc/udev/rules.d/99-esp32.rules"

sudo cp "$RULE_SRC" "$RULE_DST"
sudo udevadm control --reload
sudo udevadm trigger -s tty || true

# Add current user to dialout (first time setup)
if ! id -nG "$USER" | tr ' ' '\n' | grep -q '^dialout$'; then
  sudo usermod -a -G dialout "$USER"
  echo "Added $USER to dialout. Log out/in to apply group change."
fi

echo "Done. Now replug the device, then: ls -l /dev/esp32*"
