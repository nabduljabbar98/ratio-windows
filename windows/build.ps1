# PowerShell build script for Ratio on Windows
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $scriptDir

Write-Host "==============================================" -ForegroundColor Cyan
Write-Host "Building Ratio for Windows" -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan

cmd.exe /c "build.bat"

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed with exit code $LASTEXITCODE"
} else {
    Write-Host "`nBuild complete! Executable is at: windows\build\Ratio.exe" -ForegroundColor Green
}
