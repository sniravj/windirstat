# WinDirStat Build Instructions

This document explains how to build WinDirStat using the provided build scripts.

## Build Scripts

Four build scripts are provided for easy building:

### PowerShell Scripts (Recommended)
- **`build-debug.ps1`** - Builds in Debug mode
- **`build-release.ps1`** - Builds in Release mode

### Batch Files (Legacy/Compatibility)
- **`build-debug.bat`** - Builds in Debug mode
- **`build-release.bat`** - Builds in Release mode

## Usage

### Quick Build

**Debug Mode:**
```powershell
# PowerShell
.\build-debug.ps1

# Or using batch
build-debug.bat
```

**Release Mode:**
```powershell
# PowerShell
.\build-release.ps1

# Or using batch
build-release.bat
```

### Build Options

All scripts support the following options:

**PowerShell:**
```powershell
# Incremental build (default)
.\build-debug.ps1

# Clean only (removes build artifacts)
.\build-debug.ps1 -Clean

# Rebuild (clean + build)
.\build-debug.ps1 -Rebuild
```

**Batch:**
```cmd
# Incremental build (default)
build-debug.bat

# Clean only
build-debug.bat clean

# Rebuild (clean + build)
build-debug.bat rebuild
```

### Running from Anywhere

The scripts automatically change to the project directory, so you can run them from anywhere:

```powershell
# From anywhere on your system
powershell -File "d:\Projects\Personal\S3Explorer\windirstat-master\build-debug.ps1"

# Or with batch
"d:\Projects\Personal\S3Explorer\windirstat-master\build-debug.bat"
```

## Build Output

After a successful build, you'll find the output in the `build` directory:

- **Debug Mode:**
  - `build\WinDirStat_x64.exe` - Debug executable
  - `build\WinDirStat_x64.pdb` - Debug symbols

- **Release Mode:**
  - `build\WinDirStat_x64.exe` - Release executable
  - `build\WinDirStat_x64.pdb` - Release symbols (optimized)

## Requirements

- **Visual Studio 2022** (or later) with C++ development tools
- **Windows SDK**
- **vcpkg** (for AWS SDK and other dependencies)

The scripts will automatically:
- Find your Visual Studio installation
- Set up the build environment
- Build the project with parallel compilation (`/m`)

## HTML Export Feature

After building, WinDirStat includes an automatic HTML export feature:
- Runs automatically after each scan completes
- Exports to: `{exe_directory}\{scan_target}_{timestamp}.html`
- Includes interactive treemap, file tree, and extension summary
- Fully self-contained (no external dependencies except D3.js CDN)

## Troubleshooting

**Visual Studio not found:**
- Install Visual Studio with "Desktop development with C++" workload
- Or install "Build Tools for Visual Studio"

**C++ tools not found:**
- In Visual Studio Installer, ensure "MSVC v143" and "Windows SDK" are installed

**Build fails:**
- Try running `build-debug.bat rebuild` to do a clean rebuild
- Check that all dependencies are installed via vcpkg
- Ensure you have the latest Windows SDK

## Examples

```powershell
# Quick debug build for testing
.\build-debug.ps1

# Clean release build for distribution
.\build-release.ps1 -Rebuild

# Just clean the build artifacts
.\build-debug.ps1 -Clean
```

## Build Configurations

- **Debug**: Includes debugging symbols, no optimizations, better for development
- **Release**: Optimized, smaller executable, better for distribution

Both configurations support the HTML export feature.
