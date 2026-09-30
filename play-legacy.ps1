param([switch]$Hidden)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$config = Join-Path $root 'runtime\fceux-win64\fceux.cfg'
if (-not (Test-Path -LiteralPath $config)) { Copy-Item -LiteralPath (Join-Path $root 'runtime\windows-defaults.cfg') -Destination $config }
$rom = Join-Path $root 'Young Indiana Jones Chronicles, The (USA).nes'
$expected = 'A8CEC2954F957A88C1628AA6F6CC929BABACC38FAFA9A509A1EA0E253131D3F3'
if (-not (Test-Path -LiteralPath $rom)) { throw 'Place your Young Indiana Jones Chronicles USA NES ROM in this folder.' }
if ((Get-FileHash -LiteralPath $rom -Algorithm SHA256).Hash -ne $expected) { throw 'This co-op build requires the supported USA ROM. See README.md.' }
$working = Join-Path $root 'runtime\Indiana-Coop.nes'
$source = [IO.File]::ReadAllBytes($rom)
$expanded = New-Object byte[] ($source.Length + 131072)
[Array]::Copy($source,$expanded,$source.Length)
$expanded[5] = 32
[IO.File]::WriteAllBytes($working,$expanded)
$env:INDIANA_COOP_ROOT = $root.Replace('\','/')
$env:INDIANA_COOP_SESSION = [Guid]::NewGuid().ToString()
$diagnostics = Join-Path $root 'diagnostics'
New-Item -ItemType Directory -Force -Path $diagnostics | Out-Null
$style = if ($Hidden) { 'Hidden' } else { 'Normal' }
$process = Start-Process -FilePath (Join-Path $root 'runtime\fceux-win64\fceux64.exe') -WorkingDirectory $root -ArgumentList @('-no8lim','1','-lua',"`"$root\play.lua`"","`"$working`"") -WindowStyle $style -PassThru -RedirectStandardOutput "$diagnostics\launch-stdout.txt" -RedirectStandardError "$diagnostics\launch-stderr.txt"
Set-Content -LiteralPath "$diagnostics\launch.txt" -Value "Started native Windows FCEUX. PID=$($process.Id) Session=$env:INDIANA_COOP_SESSION"
$deadline = (Get-Date).AddSeconds(8)
$ready = Join-Path $diagnostics 'launch-ready.txt'
while ((Get-Date) -lt $deadline) {
    if ($process.HasExited) { throw "The emulator closed during startup. See $diagnostics\launch.txt and launch-stderr.txt." }
    if ((Test-Path -LiteralPath $ready) -and (Get-Content -LiteralPath $ready -Raw).Trim() -eq $env:INDIANA_COOP_SESSION) {
        Add-Content -LiteralPath "$diagnostics\launch.txt" -Value 'Ready: actual keyboard input and emulation frame completed.'
        Write-Host 'Indiana Jones Co-op is running. Press Enter in the game window to start.'
        exit 0
    }
    Start-Sleep -Milliseconds 100
}
throw "The emulator opened but the co-op script did not finish startup. See $diagnostics\launch-stderr.txt."
