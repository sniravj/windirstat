@ECHO OFF
REM WinDirStat - Release Build Script
REM This script builds WinDirStat in Release mode
REM Can be run from any directory

SETLOCAL EnableDelayedExpansion

REM Change to script directory
cd /d "%~dp0"

ECHO ========================================
ECHO WinDirStat Release Build
ECHO ========================================
ECHO.

REM Find Visual Studio using vswhere
SET "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
IF NOT EXIST "%VSWHERE%" (
    ECHO ERROR: Visual Studio not found!
    ECHO Please install Visual Studio with C++ development tools.
    EXIT /B 1
)

REM Get Visual Studio installation path
FOR /F "usebackq delims=" %%i IN (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) DO (
    SET "VSPATH=%%i"
)

IF NOT DEFINED VSPATH (
    ECHO ERROR: Visual Studio C++ tools not found!
    ECHO Please install C++ development tools in Visual Studio.
    EXIT /B 1
)

ECHO Found Visual Studio at: !VSPATH!

REM Setup Visual Studio environment
SET "VSDEVCMD=!VSPATH!\Common7\Tools\VsDevCmd.bat"
IF NOT EXIST "!VSDEVCMD!" (
    ECHO ERROR: VsDevCmd.bat not found!
    EXIT /B 1
)

REM Determine build target based on parameters
SET "BUILDTARGET=Build"
IF /I "%1"=="clean" (
    SET "BUILDTARGET=Clean"
    ECHO Build Mode: Clean only
) ELSE IF /I "%1"=="rebuild" (
    SET "BUILDTARGET=Clean;Build"
    ECHO Build Mode: Rebuild ^(Clean + Build^)
) ELSE (
    ECHO Build Mode: Incremental Build
)

ECHO Configuration: Release
ECHO Platform: x64
ECHO.

REM Build the project
ECHO Starting build...
CALL "!VSDEVCMD!" >NUL 2>&1
msbuild windirstat.sln /p:Configuration=Release /p:Platform=x64 /t:!BUILDTARGET! /m /v:minimal

IF %ERRORLEVEL% EQU 0 (
    ECHO.
    ECHO ========================================
    ECHO BUILD SUCCESSFUL!
    ECHO ========================================
    ECHO.
    ECHO Output: build\WinDirStat_x64.exe
    ECHO PDB: build\WinDirStat_x64.pdb
) ELSE (
    ECHO.
    ECHO ========================================
    ECHO BUILD FAILED!
    ECHO ========================================
    EXIT /B %ERRORLEVEL%
)

ENDLOCAL
