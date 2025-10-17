# VibeNES Debug Build Script
# Automatically discovers and compiles all source files

$ErrorActionPreference = "Stop"

# Paths
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build\debug"
$Compiler = "c:\msys64\ucrt64\bin\g++.exe"

Write-Host "=== VibeNES Debug Build ===" -ForegroundColor Cyan
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

# Compiler flags
$CompilerFlags = @(
    "-fdiagnostics-color=always",
    "-g3",
    "-O0",
    "-std=c++23",
    "-Wall",
    "-Wextra"
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
$OutputFile = Join-Path $BuildDir "VibeNES_GUI.exe"

# Build command
Write-Host "`nBuilding..." -ForegroundColor Cyan
$CompileCommand = @($Compiler) + $CompilerFlags + $AllFiles + $IncludePaths + $LinkerFlags + $Defines + @("-o", $OutputFile)

# Execute compilation
try {
    & $CompileCommand[0] $CompileCommand[1..($CompileCommand.Length-1)]

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
