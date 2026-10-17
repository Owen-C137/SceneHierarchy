# Deploys the Release build to <game>\bin\plugins\<Plugin>, only while no mod tool is running, after backing up what
# it overwrites to .\deployment-backups\<stamp>. Build first (Release|x64). Verifies the copied files by hash.
param(
    [string]$Bo3Root = "D:\SteamLibrary\steamapps\common\Call of Duty Black Ops III",
    [switch]$WhatIf
)
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$name = Split-Path -Leaf $here
$deploy = Join-Path $Bo3Root "bin\plugins\$name"
$files = @("bin\Release\$name.dll", "bin\Release\$name.pdb", "README.md") | ForEach-Object { Join-Path $here $_ }

$running = Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^(Radiant_modtools|asseteditor_modtools)$' }
if ($running) { throw "Close the mod tools first: $(($running | ForEach-Object { "$($_.Name) ($($_.Id))" }) -join ', ') is running." }
# The launcher only runs launcher plugins; this one is Radiant-only, so the launcher being open is fine.
if (Get-Process -Name modlauncher -ErrorAction SilentlyContinue) { Write-Host "Note: the mod launcher is open (it does not run this Radiant-only plugin)." }
foreach ($f in $files) { if (!(Test-Path -LiteralPath $f)) { throw "Missing $f (build Release|x64 first)." } }

if (Test-Path -LiteralPath $deploy) {
    $backup = Join-Path $here ("deployment-backups\" + (Get-Date -Format "yyyyMMdd-HHmmss"))
    Write-Host "Backing up $deploy to $backup"
    if (!$WhatIf) { New-Item -ItemType Directory -Force -Path $backup | Out-Null; Copy-Item -Path (Join-Path $deploy "*") -Destination $backup -Recurse -Force }
} else {
    Write-Host "First deploy: $deploy does not exist yet (nothing to back up)"
}
if ($WhatIf) { Write-Host "WhatIf: would copy $($files -join ', ') to $deploy"; return }
New-Item -ItemType Directory -Force -Path $deploy | Out-Null
foreach ($f in $files) {
    $target = Join-Path $deploy (Split-Path -Leaf $f)
    Copy-Item -LiteralPath $f -Destination $target -Force
    if ((Get-FileHash -LiteralPath $f).Hash -ne (Get-FileHash -LiteralPath $target).Hash) { throw "Hash mismatch after copying $f" }
    Write-Host "Deployed $(Split-Path -Leaf $f) ($((Get-FileHash -LiteralPath $target).Hash.Substring(0, 12)))"
}
