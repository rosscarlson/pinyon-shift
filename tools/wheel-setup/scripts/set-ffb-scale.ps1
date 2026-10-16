# Sets the wheel force feedback scale Pinyon Shift uses for FH1's effects (wheel_ffb_magnitude_max).
# FH1 sends its effect values in a byte range (its spring showed coefficient 63, saturation 127),
# not DirectInput's 10000, so without this the forces come out at about 1 % strength.
#   powershell -ExecutionPolicy Bypass -File set-ffb-scale.ps1 [-Install "G:\Games\Forza Horizon 1"] [-Value 127]
# Close the game first: it saves its own settings over the config on exit.
param(
    [string]$Install = 'G:\Games\Forza Horizon 1',
    [int]$Value = 127
)
$ErrorActionPreference = 'Stop'

$config = Join-Path $Install 'source\0.1.1\.local\preview\config\pinyon_shift.toml'
if (-not (Test-Path $config)) { throw "Config not found: $config (pass -Install with your install folder)" }
if (Get-Process -Name pinyon_shift -ErrorAction SilentlyContinue) { throw 'Close Pinyon Shift first, then run this again.' }

$backup = "$config.ffb-scale-backup-$(Get-Date -Format yyyyMMdd-HHmmss)"
Copy-Item $config $backup

$text = [IO.File]::ReadAllText($config)
$newline = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
$lines = [Collections.Generic.List[string]]::new()
foreach ($line in ($text -split "`r?`n")) {
    if ($line -notmatch '^\s*wheel_ffb_magnitude_max\s*=') { $lines.Add($line) }
}
while ($lines.Count -gt 0 -and $lines[$lines.Count - 1] -eq '') { $lines.RemoveAt($lines.Count - 1) }
# A top-level key must come before any [table] header.
$table = -1
for ($i = 0; $i -lt $lines.Count; $i++) { if ($lines[$i].TrimStart().StartsWith('[')) { $table = $i; break } }
$entry = "wheel_ffb_magnitude_max = $Value"
if ($table -ge 0) { $lines.Insert($table, $entry) } else { $lines.Add($entry) }
[IO.File]::WriteAllText($config, ($lines -join $newline) + $newline, [Text.UTF8Encoding]::new($false))

Write-Host "Set $entry in $config"
Write-Host "Backup: $backup"
Select-String -Path $config -Pattern '^\s*wheel_ffb_(magnitude_max|strength|device|mode|trace)\s*=' | ForEach-Object { Write-Host "  $($_.Line)" }
