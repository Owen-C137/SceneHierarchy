# Builds and runs the HierarchyModel unit checks (Radiance's hierarchy rules and store format).
param([string]$Work = (Join-Path $env:TEMP "SceneHierarchyTests"))
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
