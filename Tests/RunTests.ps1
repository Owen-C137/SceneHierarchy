# Builds and runs the unit checks: HierarchyModel (Radiance's hierarchy rules and store format) and the glyph rasterizer.
# Pass glyph names after -Show to print them as text art.
param([string]$Work = (Join-Path $env:TEMP "SceneHierarchyTests"), [string[]]$Show = @())
$ErrorActionPreference = "Continue"
$tests = Split-Path -Parent $MyInvocation.MyCommand.Path
$plugin = Split-Path -Parent $tests
New-Item -ItemType Directory -Force -Path $Work | Out-Null
$installer = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer"
$env:PATH = "$installer;$env:PATH"
$vs = & (Join-Path $installer "vswhere.exe") -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw "Visual Studio with the C++ tools was not found." }
$vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"

$exe = Join-Path $Work "HierarchyModelTests.exe"
$sources = @("Tests\HierarchyModelTests.cpp", "HierarchyModel.cpp") | ForEach-Object { "`"$(Join-Path $plugin $_)`"" }
cmd /c "`"$vcvars`" >nul && cl /nologo /std:c++17 /O2 /EHsc /W3 /DNOMINMAX /DWIN32_LEAN_AND_MEAN /Fe`"$exe`" /Fo`"$Work\\`" $($sources -join ' ') bcrypt.lib shell32.lib ole32.lib"
if ($LASTEXITCODE -ne 0) { throw "Test build failed." }
& $exe
if ($LASTEXITCODE -ne 0) { throw "Unit checks failed." }

$glyphExe = Join-Path $Work "GlyphTests.exe"
cmd /c "`"$vcvars`" >nul && cl /nologo /std:c++17 /O2 /EHsc /W3 /DNOMINMAX /Fe`"$glyphExe`" /Fo`"$Work\\`" `"$(Join-Path $plugin 'Tests\GlyphTests.cpp')`""
if ($LASTEXITCODE -ne 0) { throw "Glyph test build failed." }
& $glyphExe @Show
if ($LASTEXITCODE -ne 0) { throw "Glyph checks failed." }
