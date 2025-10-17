# VibeNES Test Build Script
# Automatically discovers and compiles all test and source files

$ErrorActionPreference = "Stop"

# Paths
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build\debug"
$Compiler = "c:\msys64\ucrt64\bin\g++.exe"

Write-Host "=== VibeNES Test Build ===" -ForegroundColor Cyan
Write-Host "Project Root: $ProjectRoot" -ForegroundColor Gray

# Create build directory if it doesn't exist
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
    Write-Host "Created build directory: $BuildDir" -ForegroundColor Green
}

# Discover source files
Write-Host "`nDiscovering files..." -ForegroundColor Cyan

# Test files (include test_main.cpp and all test files)
$TestMainFile = Join-Path $ProjectRoot "tests\test_main.cpp"
$Catch2File = Join-Path $ProjectRoot "tests\catch2\catch_amalgamated.cpp"
$TestFiles = Get-ChildItem -Path (Join-Path $ProjectRoot "tests") -Recurse -Filter "*.cpp" |
    Where-Object { $_.FullName -ne $TestMainFile -and $_.FullName -ne $Catch2File } |
    ForEach-Object { $_.FullName }

# Source files (exclude main.cpp - tests have their own main)
$MainFile = Join-Path $ProjectRoot "src\main.cpp"
$SourceFiles = Get-ChildItem -Path (Join-Path $ProjectRoot "src") -Recurse -Filter "*.cpp" |
    Where-Object { $_.FullName -ne $MainFile } |
    ForEach-Object { $_.FullName }

# Combine all files
$AllFiles = @($TestMainFile, $Catch2File) + $TestFiles + $SourceFiles

Write-Host "Found $($TestFiles.Count) test files" -ForegroundColor Green
Write-Host "Found $($SourceFiles.Count) source files" -ForegroundColor Green
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
    "-I$ProjectRoot\tests"
)

# Output
$OutputFile = Join-Path $BuildDir "VibeNES_All_Tests.exe"

# Build command
Write-Host "`nBuilding tests..." -ForegroundColor Cyan
$CompileCommand = @($Compiler) + $CompilerFlags + $AllFiles + $IncludePaths + @("-o", $OutputFile)

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
