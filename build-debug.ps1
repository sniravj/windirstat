# WinDirStat - Debug Build Script
# This script builds WinDirStat in Debug mode
# Can be run from any directory

param(
    [switch]$Clean,
    [switch]$Rebuild
)

# Get the directory where this script is located
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "WinDirStat Debug Build" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Find Visual Studio using vswhere
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Host "ERROR: Visual Studio not found!" -ForegroundColor Red
    Write-Host "Please install Visual Studio with C++ development tools." -ForegroundColor Red
    exit 1
}

# Get Visual Studio installation path
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) {
    Write-Host "ERROR: Visual Studio C++ tools not found!" -ForegroundColor Red
    Write-Host "Please install C++ development tools in Visual Studio." -ForegroundColor Red
    exit 1
}

Write-Host "Found Visual Studio at: $vsPath" -ForegroundColor Green

# Setup Visual Studio environment
$vsDevCmd = Join-Path $vsPath "Common7\Tools\VsDevCmd.bat"
if (-not (Test-Path $vsDevCmd)) {
    Write-Host "ERROR: VsDevCmd.bat not found!" -ForegroundColor Red
    exit 1
}

# Determine build target
$buildTarget = "Build"
if ($Clean) {
    $buildTarget = "Clean"
    Write-Host "Build Mode: Clean only" -ForegroundColor Yellow
} elseif ($Rebuild) {
    $buildTarget = "Clean;Build"
    Write-Host "Build Mode: Rebuild (Clean + Build)" -ForegroundColor Yellow
} else {
    Write-Host "Build Mode: Incremental Build" -ForegroundColor Yellow
}

Write-Host "Configuration: Debug" -ForegroundColor Yellow
Write-Host "Platform: x64" -ForegroundColor Yellow
Write-Host ""

# Build the project
Write-Host "Starting build..." -ForegroundColor Cyan
$buildCmd = "`"$vsDevCmd`" && msbuild windirstat.sln /p:Configuration=Debug /p:Platform=x64 /t:$buildTarget /m /v:minimal"

$process = Start-Process cmd.exe -ArgumentList "/c", $buildCmd -NoNewWindow -Wait -PassThru

if ($process.ExitCode -eq 0) {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "BUILD SUCCESSFUL!" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "Output: build\WinDirStat_x64.exe" -ForegroundColor Green
    Write-Host "PDB: build\WinDirStat_x64.pdb" -ForegroundColor Green
} else {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Red
    Write-Host "BUILD FAILED!" -ForegroundColor Red
    Write-Host "========================================" -ForegroundColor Red
    exit $process.ExitCode
}
