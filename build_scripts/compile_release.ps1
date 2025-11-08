# VibeNES Release Build Script
# Automatically discovers and compiles all source files with optimizations

$ErrorActionPreference = "Stop"

# Paths
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build\release"
$Compiler = "c:\msys64\ucrt64\bin\g++.exe"

Write-Host "=== VibeNES Release Build ===" -ForegroundColor Cyan
Write-Host "Project Root: $ProjectRoot" -ForegroundColor Gray

# Create build directory if it doesn't exist
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
    Write-Host "Created build directory: $BuildDir" -ForegroundColor Green
}

# Discover source files
Write-Host "`nDiscovering source files..." -ForegroundColor Cyan

$MainFile = Join-Path $ProjectRoot "src\main.cpp"
$SourceFiles = Get-ChildItem -Path (Join-Path $ProjectRoot "src") -Recurse -Filter "*.cpp" |
    Where-Object { $_.FullName -ne $MainFile } |
    ForEach-Object { $_.FullName }

$ImGuiFiles = @(
    "third_party\imgui\imgui.cpp",
    "third_party\imgui\imgui_demo.cpp",
    "third_party\imgui\imgui_draw.cpp",
    "third_party\imgui\imgui_tables.cpp",
    "third_party\imgui\imgui_widgets.cpp",
    "third_party\imgui\backends\imgui_impl_sdl2.cpp",
    "third_party\imgui\backends\imgui_impl_opengl3.cpp"
) | ForEach-Object { Join-Path $ProjectRoot $_ }

# Combine all files (main.cpp first, then all other sources, then ImGui)
$AllFiles = @($MainFile) + $SourceFiles + $ImGuiFiles

Write-Host "Found $($SourceFiles.Count) project source files" -ForegroundColor Green
Write-Host "Found $($ImGuiFiles.Count) ImGui source files" -ForegroundColor Green
Write-Host "Total: $($AllFiles.Count) files to compile" -ForegroundColor Yellow

# Compiler flags (optimized for release)
$CompilerFlags = @(
    "-fdiagnostics-color=always",
    "-O3",
    "-DNDEBUG",
    "-march=native",
    "-Wall",
    "-Wextra",
    "-std=c++23"
)

# ImGui-specific flags (disable -Weffc++ for third-party code)
$ImGuiFlags = @(
    "-fdiagnostics-color=always",
    "-O3",
    "-DNDEBUG",
    "-march=native",
    "-Wall",
    "-Wextra",
    "-std=c++23"
)

# Include paths
$IncludePaths = @(
    "-I$ProjectRoot\include",
    "-I$ProjectRoot\third_party\imgui",
    "-I$ProjectRoot\third_party\imgui\backends",
    "-IC:\msys64\ucrt64\include\SDL2",
    "-IC:\msys64\ucrt64\include"
)

# Library paths and libraries
$LinkerFlags = @(
    "-LC:\msys64\ucrt64\lib",
    "-lmingw32",
    "-lSDL2main",
    "-lSDL2",
    "-lSDL2_image",
    "-lSDL2_ttf",
    "-lopengl32"
)

# Defines
$Defines = @(
    "-DNES_GUI_ENABLED"
)

# Output
$OutputFile = Join-Path $BuildDir "VibeNES.exe"

# Compile ImGui files separately with relaxed warnings
Write-Host "`nCompiling ImGui libraries..." -ForegroundColor Cyan
$ImGuiObjects = @()
foreach ($ImGuiFile in $ImGuiFiles) {
    $ObjectFile = Join-Path $BuildDir ([System.IO.Path]::GetFileNameWithoutExtension($ImGuiFile) + ".o")
    $ImGuiObjects += $ObjectFile

    $ImGuiCompileCmd = @($Compiler) + $ImGuiFlags + @("-c", $ImGuiFile) + $IncludePaths + $Defines + @("-o", $ObjectFile)
    & $ImGuiCompileCmd[0] $ImGuiCompileCmd[1..($ImGuiCompileCmd.Length-1)]

    if ($LASTEXITCODE -ne 0) {
        Write-Host "Failed to compile $ImGuiFile" -ForegroundColor Red
        exit $LASTEXITCODE
    }
}

# Compile project files with strict warnings
Write-Host "`nCompiling project files..." -ForegroundColor Cyan
$ProjectFiles = @($MainFile) + $SourceFiles
$ProjectCompileCmd = @($Compiler) + $CompilerFlags + $ProjectFiles + $ImGuiObjects + $IncludePaths + $LinkerFlags + $Defines + @("-o", $OutputFile)

# Build command
Write-Host "`nLinking..." -ForegroundColor Cyan

# Execute compilation
try {
    & $ProjectCompileCmd[0] $ProjectCompileCmd[1..($ProjectCompileCmd.Length-1)]

    if ($LASTEXITCODE -eq 0) {
        Write-Host "`n=== Build Successful ===" -ForegroundColor Green
        Write-Host "Output: $OutputFile" -ForegroundColor Green

        # Display file size
        $FileInfo = Get-Item $OutputFile
        $SizeMB = [math]::Round($FileInfo.Length / 1MB, 2)
        Write-Host "Size: $SizeMB MB" -ForegroundColor Gray

        exit 0
    } else {
        Write-Host "`n=== Build Failed ===" -ForegroundColor Red
        exit $LASTEXITCODE
    }
} catch {
    Write-Host "`nError: $_" -ForegroundColor Red
    exit 1
}
