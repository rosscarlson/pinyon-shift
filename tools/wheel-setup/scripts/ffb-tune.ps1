# Changes Pinyon Shift's wheel force feedback settings in the game config (close the game first).
#   powershell -ExecutionPolicy Bypass -File ffb-tune.ps1                     # show the current settings
#   powershell -ExecutionPolicy Bypass -File ffb-tune.ps1 -Invert on          # reverse the force direction
#   powershell -ExecutionPolicy Bypass -File ffb-tune.ps1 -Rumble 0           # no kerb/impact vibration
#   powershell -ExecutionPolicy Bypass -File ffb-tune.ps1 -Strength 30 -Scale 127
# Options can be combined. A backup of the config is kept each time something changes.
param(
    [string]$Install = 'G:\Games\Forza Horizon 1',
    [ValidateSet('on', 'off')][string]$Invert,
    [ValidateRange(0, 100)][int]$Rumble = -1,
    [ValidateRange(0, 100)][int]$Strength = -1,
    [ValidateRange(1, 1000000)][int]$Scale = 0,
    [ValidateSet('on', 'off')][string]$Trace
)
$ErrorActionPreference = 'Stop'
$config = Join-Path $Install 'source\0.1.1\.local\preview\config\pinyon_shift.toml'
if (-not (Test-Path $config)) { throw "Config not found: $config (pass -Install with your install folder)" }

$changes = [ordered]@{}
if ($Invert) { $changes['wheel_ffb_invert'] = if ($Invert -eq 'on') { 'true' } else { 'false' } }
if ($Rumble -ge 0) { $changes['wheel_ffb_rumble'] = "$Rumble" }
if ($Strength -ge 0) { $changes['wheel_ffb_strength'] = "$Strength" }
if ($Scale -gt 0) { $changes['wheel_ffb_magnitude_max'] = "$Scale" }
if ($Trace) { $changes['wheel_ffb_trace'] = if ($Trace -eq 'on') { 'true' } else { 'false' } }

if ($changes.Count -gt 0) {
    if (Get-Process -Name pinyon_shift -ErrorAction SilentlyContinue) { throw 'Close Pinyon Shift first, then run this again.' }
    Copy-Item $config "$config.ffb-tune-backup-$(Get-Date -Format yyyyMMdd-HHmmss)"
    $text = [IO.File]::ReadAllText($config)
    $newline = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
    $lines = [Collections.Generic.List[string]]::new()
    foreach ($line in ($text -split "`r?`n")) {
        $key = if ($line -match '^\s*([A-Za-z0-9_]+)\s*=') { $Matches[1] } else { $null }
        if (-not ($key -and $changes.Contains($key))) { $lines.Add($line) }
    }
    while ($lines.Count -gt 0 -and $lines[$lines.Count - 1] -eq '') { $lines.RemoveAt($lines.Count - 1) }
    $table = -1
    for ($i = 0; $i -lt $lines.Count; $i++) { if ($lines[$i].TrimStart().StartsWith('[')) { $table = $i; break } }
    $at = if ($table -ge 0) { $table } else { $lines.Count }
    foreach ($key in $changes.Keys) { $lines.Insert($at, "$key = $($changes[$key])"); $at++ }
    [IO.File]::WriteAllText($config, ($lines -join $newline) + $newline, [Text.UTF8Encoding]::new($false))
    Write-Host "Updated $config"
}

Write-Host 'Force feedback settings (missing ones use the defaults: invert false, rumble 50, strength 25, scale 10000, trace false):'
Select-String -Path $config -Pattern '^\s*wheel_ffb_[a-z_]+\s*=' | ForEach-Object { Write-Host "  $($_.Line)" }
