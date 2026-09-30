param([switch]$Hidden)
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$exe=Join-Path $root 'Indiana-Coop.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw 'Indiana-Coop.exe is missing. Run build.ps1 or restore the native build.' }
$diagnostics=Join-Path $root 'diagnostics'
New-Item -ItemType Directory -Force -Path $diagnostics | Out-Null
$env:INDIANA_COOP_SESSION=[Guid]::NewGuid().ToString()
$style=if ($Hidden) { 'Hidden' } else { 'Normal' }
$process=Start-Process -FilePath $exe -WorkingDirectory $root -WindowStyle $style -PassThru -RedirectStandardOutput "$diagnostics\native-launch-stdout.txt" -RedirectStandardError "$diagnostics\native-launch-stderr.txt"
Set-Content -LiteralPath "$diagnostics\launch.txt" -Value "Native core launcher PID=$($process.Id) Session=$env:INDIANA_COOP_SESSION"
$deadline=(Get-Date).AddSeconds(8)
$ready=Join-Path $diagnostics 'native-ready.txt'
while ((Get-Date) -lt $deadline) {
    if ($process.HasExited) { throw 'Native game closed during startup. See diagnostics/native-launch-stderr.txt.' }
    if ((Test-Path -LiteralPath $ready) -and (Get-Content -LiteralPath $ready -Raw).Trim() -eq $env:INDIANA_COOP_SESSION) {
        Add-Content -LiteralPath "$diagnostics\launch.txt" -Value 'Ready: native core produced its first frame.'
        Write-Host 'Indiana Co-op is running. Enter starts the game; F2 changes controls.'
        exit 0
    }
    Start-Sleep -Milliseconds 100
}
throw 'Native startup did not finish. See diagnostics/native-launch-stderr.txt.'
