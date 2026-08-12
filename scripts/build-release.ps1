$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root 'build-nmake'
$vsDev = 'C:\BuildTools\Common7\Tools\VsDevCmd.bat'
$cmake = 'C:\Program Files\CMake\bin\cmake.exe'

if (-not (Test-Path $vsDev)) { throw "Visual Studio build tools were not found: $vsDev" }
if (-not (Test-Path $cmake)) { throw "CMake was not found: $cmake" }

$command = "call `"$vsDev`" -arch=x64 -host_arch=x64 && `"$cmake`" -S `"$root`" -B `"$build`" -G `"NMake Makefiles`" && `"$cmake`" --build `"$build`" && `"$cmake`" --build `"$build`" --target test"
cmd /c $command
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$release = Join-Path $root 'build\Release'
New-Item -ItemType Directory -Force -Path $release | Out-Null
Copy-Item (Join-Path $build 'SysGlance.exe') (Join-Path $release 'SysGlance.exe') -Force
