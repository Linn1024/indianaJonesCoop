param([string]$ToolchainBin=$env:INDIANA_MINGW_BIN,[switch]$CoreOnly)
$ErrorActionPreference='Stop'
if (-not $ToolchainBin) {
    $localToolchain='C:\TEMP2\zeroTolerance\bizhawk-integration\mingw64\bin'
    if (Test-Path -LiteralPath "$localToolchain\gcc.exe") { $ToolchainBin=$localToolchain }
}
$oldPath=$env:PATH
Push-Location $PSScriptRoot
try {
    if ($ToolchainBin) { $env:PATH="$ToolchainBin;$oldPath" }
    if ((& gcc -dumpmachine) -notmatch '^x86_64-') { throw 'Use a 64-bit MinGW-w64 toolchain: -ToolchainBin C:\path\to\mingw64\bin' }
    & mingw32-make -C engine -f Makefile.libretro platform=win CC=gcc HAVE_HDPACK=0 HAVE_NTSC=0 WANT_32BPP=1 GIT_VERSION= -j8 -s
    if ($LASTEXITCODE) { throw 'Native core build failed' }
    if (-not $CoreOnly) {
        & g++ launcher.cpp -o Indiana-Coop.exe -O2 -std=c++17 -static -mwindows -lwinmm -lgdi32 -luser32 -lbcrypt -lshell32
        if ($LASTEXITCODE) { throw 'Launcher build failed' }
    }
} finally { $env:PATH=$oldPath; Pop-Location }
