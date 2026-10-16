# Collects what's needed to check the racing wheel build into one ZIP on your Desktop:
# the wheel/FFB lines of the latest runtime log, the newest FFB trace, the wheel_* config
# lines and the launcher's SDK pin. No game files or saves are included.
#   powershell -ExecutionPolicy Bypass -File collect-wheel-logs.ps1 [-Install "G:\Games\Forza Horizon 1"]
param([string]$Install = 'G:\Games\Forza Horizon 1')

$source = Join-Path $Install 'source\0.1.1'
$preview = Join-Path $source '.local\preview'
$logs = Join-Path $preview 'logs'
$config = Join-Path $preview 'config\pinyon_shift.toml'
if (-not (Test-Path $logs)) { throw "Logs not found: $logs (pass -Install with your install folder)" }

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$work = Join-Path $env:TEMP "pinyon-wheel-logs-$stamp"
New-Item -ItemType Directory -Force $work | Out-Null

$summary = New-Object System.Collections.Generic.List[string]
$summary.Add("Collected $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') from $source")

$toolchain = Join-Path $source 'config\release-toolchain.json'
if (Test-Path $toolchain) {
    $summary.Add('SDK pin: ' + ((Select-String -Path $toolchain -Pattern '"repository"|"revision"' | ForEach-Object { $_.Line.Trim() }) -join ' '))
}
$built = Join-Path $source '.local\rexglue\.git\HEAD'
if (Test-Path $built) { $summary.Add('SDK last built: ' + (Get-Content $built -Raw).Trim()) }

$runtime = Join-Path $logs 'runtime.log'
if (Test-Path $runtime) {
    Select-String -Path $runtime -Pattern 'Wheel FFB|SDL wheel|SDL input|FFB trace|XInputd|STUB|Window closing|Title terminated|error' |
        ForEach-Object { $_.Line } | Select-Object -Last 400 | Set-Content (Join-Path $work 'runtime-wheel-lines.log') -Encoding utf8
    Get-Content $runtime -Tail 150 | Set-Content (Join-Path $work 'runtime-tail.log') -Encoding utf8
    $summary.Add("runtime.log: $((Get-Item $runtime).LastWriteTime)")
} else { $summary.Add('runtime.log: not found') }

$trace = Get-ChildItem $logs -Filter 'ffb-trace-*.csv' -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if ($trace) {
    Copy-Item $trace.FullName $work
    $summary.Add("FFB trace: $($trace.Name), $([math]::Round($trace.Length / 1KB)) KB")
} else { $summary.Add('FFB trace: none (turn on Record force feedback trace)') }

if (Test-Path $config) {
    Select-String -Path $config -Pattern '^\s*(wheel_|input_backend|pad_rumble)' | ForEach-Object { $_.Line } |
        Set-Content (Join-Path $work 'config-wheel-lines.toml') -Encoding utf8
} else { $summary.Add("Config not found: $config") }

$summary | Set-Content (Join-Path $work 'summary.txt') -Encoding utf8
$zip = Join-Path ([Environment]::GetFolderPath('Desktop')) "pinyon-wheel-logs-$stamp.zip"
Compress-Archive -Path (Join-Path $work '*') -DestinationPath $zip -Force
Remove-Item $work -Recurse -Force
$summary | ForEach-Object { Write-Host $_ }
Write-Host ''
Write-Host "Saved $zip"
