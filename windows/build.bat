@echo off
setlocal enabledelayedexpansion

echo ==============================================
echo Building Ratio for Windows
echo ==============================================

cd /d "%~dp0"

:: Find vcvars64.bat
set "VCVARS="
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
)

:: Fallback to vswhere locator if not in default paths
if "%VCVARS%"=="" (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if exist "!VSWHERE!" (
        for /f "usebackq delims=" %%i in (`"!VSWHERE!" -latest -products * -property installationPath`) do (
            if exist "%%i\VC\Auxiliary\Build\vcvars64.bat" (
                set "VCVARS=%%i\VC\Auxiliary\Build\vcvars64.bat"
            )
        )
    )
)

if "%VCVARS%"=="" (
    echo ERROR: Visual Studio 2022 C++ tools not found.
    exit /b 1
)

echo Initializing MSVC environment: %VCVARS%
call "%VCVARS%" >nul 2>&1
if errorlevel 1 (
    echo ERROR: Failed to configure MSVC environment.
    exit /b 1
)

if not exist "build" mkdir "build"

echo Compiling Windows resources...
rc.exe /nologo /fo "build\resource.res" "res\resource.rc"
if errorlevel 1 (
    echo ERROR: Resource compilation failed.
    exit /b 1
)

echo Compiling Ratio.exe (Windows Tray GUI App)...
cl.exe /nologo /std:c++17 /O2 /W3 /EHsc /DUNICODE /D_UNICODE /DNOMINMAX /I res /I src "src\main.cpp" /Fo"build\\" /Fe"build\Ratio.exe" "build\resource.res" /link /SUBSYSTEM:WINDOWS
if errorlevel 1 (
    echo ERROR: Build of Ratio.exe failed.
    exit /b 1
)

echo Compiling RatioTest.exe (Console Self-Test Runner)...
cl.exe /nologo /std:c++17 /O2 /W3 /EHsc /DNOMINMAX /I src "src\test_main.cpp" /Fo"build\\" /Fe"build\RatioTest.exe" /link /SUBSYSTEM:CONSOLE
if errorlevel 1 (
    echo ERROR: Build of RatioTest.exe failed.
    exit /b 1
)

echo.
echo Running accounting self-tests...
"build\RatioTest.exe"
if errorlevel 1 (
    echo ERROR: Self-tests failed!
    exit /b 1
)

echo.
echo ==============================================
echo Ratio for Windows is ready: windows\build\Ratio.exe
echo ==============================================
exit /b 0
