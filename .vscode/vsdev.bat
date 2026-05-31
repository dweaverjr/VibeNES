@echo off
:: Dynamically locate the latest MSVC installation (any version / edition).
:: Strategy 1: vswhere (may not yet know about VS 2026 / v18 installs)
for /f "usebackq delims=" %%i in (
    `"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" ^
        -latest ^
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 ^
        -property installationPath 2^>nul`
) do set "VS_INSTALL=%%i"

:: Strategy 2: if vswhere came up empty, walk known version folders newest-first
if not defined VS_INSTALL (
    for %%v in (18 17 16) do (
        if not defined VS_INSTALL (
            for %%e in (Enterprise Professional Community BuildTools) do (
                if not defined VS_INSTALL (
                    if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\%%v\%%e\VC\Auxiliary\Build\vcvarsall.bat" (
                        set "VS_INSTALL=%ProgramFiles(x86)%\Microsoft Visual Studio\%%v\%%e"
                    )
                )
            )
        )
    )
)
:: Also check Program Files (non-x86) for newer installs
if not defined VS_INSTALL (
    for %%v in (18 17 16) do (
        if not defined VS_INSTALL (
            for %%e in (Enterprise Professional Community BuildTools) do (
                if not defined VS_INSTALL (
                    if exist "%ProgramFiles%\Microsoft Visual Studio\%%v\%%e\VC\Auxiliary\Build\vcvarsall.bat" (
                        set "VS_INSTALL=%ProgramFiles%\Microsoft Visual Studio\%%v\%%e"
                    )
                )
            )
        )
    )
)

if not defined VS_INSTALL (
    echo [vsdev.bat] ERROR: No MSVC installation with C++ tools found.
    echo             Install "Desktop development with C++" workload via VS Installer.
    exit /b 1
)

:: Initialize the MSVC developer environment (x64)
call "%VS_INSTALL%\VC\Auxiliary\Build\vcvarsall.bat" amd64

:: Add the CMake and Ninja bundled with this VS install to PATH
set "NINJA_EXE=%VS_INSTALL%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
set "PATH=%VS_INSTALL%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VS_INSTALL%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
