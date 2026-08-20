param(
    [Parameter(Mandatory = $true)]
    [string]$Version
)

$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root 'build-portable'
$vsDev = 'C:\BuildTools\Common7\Tools\VsDevCmd.bat'
$cmake = 'C:\Program Files\CMake\bin\cmake.exe'
$packageName = "SysGlance-$Version-win-x64"
$dist = Join-Path $root 'dist'
$staging = Join-Path $dist $packageName
$archive = Join-Path $dist "$packageName.zip"

if (-not (Test-Path $vsDev)) { throw "Visual Studio build tools were not found: $vsDev" }
if (-not (Test-Path $cmake)) { throw "CMake was not found: $cmake" }
if ((Test-Path $staging) -or (Test-Path $archive)) {
    throw "Release output already exists. Choose a new version or remove: $staging"
}

$command = "call `"$vsDev`" -arch=x64 -host_arch=x64 && `"$cmake`" -S `"$root`" -B `"$build`" -G `"NMake Makefiles`" -DCMAKE_BUILD_TYPE=Release && `"$cmake`" --build `"$build`" && `"$cmake`" --build `"$build`" --target test"
cmd /c $command
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

New-Item -ItemType Directory -Path $staging | Out-Null
Copy-Item (Join-Path $build 'SysGlance.exe') (Join-Path $staging 'SysGlance.exe')
Copy-Item (Join-Path $root 'packaging\README.txt') (Join-Path $staging 'README.txt')
Compress-Archive -Path $staging -DestinationPath $archive -CompressionLevel Optimal
Write-Host "Created: $archive"
