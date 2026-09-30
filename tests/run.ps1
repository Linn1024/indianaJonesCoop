param([ValidateSet('verify','boot','campaign','frontend','raw_frontend','av','features','frontend_menu')][string]$Suite = 'verify')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$env:INDIANA_COOP_ROOT = $root.Replace('\','/')
$report = if ($Suite -eq 'verify') { 'verification' } else { $Suite }
Set-Content -LiteralPath "$root\diagnostics\$report.txt" -Value 'RUNNING'
$process = Start-Process -FilePath "$root\runtime\fceux-win64\fceux64.exe" -ArgumentList @('-no8lim','1','-lua',"`"$root\tests\$Suite.lua`"","`"$root\runtime\Indiana-Coop.nes`"") -WindowStyle Hidden -PassThru -RedirectStandardError "$root\diagnostics\$Suite-stderr.txt" -RedirectStandardOutput "$root\diagnostics\$Suite-stdout.txt"
if (-not $process.WaitForExit(55000)) { Stop-Process -Id $process.Id; throw 'Verification timed out; inspect diagnostics.' }
Get-Content "$root\diagnostics\$report.txt"
if (Select-String -Path "$root\diagnostics\$report.txt" -Pattern '^FAIL ' -Quiet) { throw "$Suite verification failed." }
if ($Suite -eq 'verify') {
    if (-not (Select-String -Path "$root\diagnostics\$report.txt" -Pattern '^ALL CHECKS PASSED$' -Quiet)) { throw 'Co-op verification failed.' }
} elseif ($Suite -ne 'campaign') {
    if (-not (Select-String -Path "$root\diagnostics\$report.txt" -Pattern '^PASS ' -Quiet)) { throw "$Suite verification failed." }
}
