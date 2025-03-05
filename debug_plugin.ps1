# Script to build the plugin and launch AudioPluginHost with the filtergraph preset

# First, build the plugin
Write-Host "Building the plugin..." -ForegroundColor Cyan
cmake --build vs-build --target AudioPlugin_VST3 --config Debug

# Check if the build was successful
if ($LASTEXITCODE -eq 0) {
    Write-Host "Build successful! Launching AudioPluginHost..." -ForegroundColor Green
    
    # Path to AudioPluginHost and filtergraph
    $hostPath = "$PSScriptRoot\AudioFilePlayer\build-host\AudioPluginHost_build\AudioPluginHost_artefacts\Debug\AudioPluginHost.exe"
    $graphPath = "$PSScriptRoot\Plug-in-host-preset.filtergraph"
    
    # Check if files exist
    if (-not (Test-Path $hostPath)) {
        Write-Host "Error: AudioPluginHost.exe not found at $hostPath" -ForegroundColor Red
        exit 1
    }
    
    if (-not (Test-Path $graphPath)) {
        Write-Host "Error: Filtergraph file not found at $graphPath" -ForegroundColor Red
        exit 1
    }
    
    # Launch AudioPluginHost
    Start-Process -FilePath $hostPath -ArgumentList "`"$graphPath`""
    
    Write-Host "AudioPluginHost launched. Your plugin should be loaded from the filtergraph preset." -ForegroundColor Green
} else {
    Write-Host "Build failed. Please fix any build errors and try again." -ForegroundColor Red
} 