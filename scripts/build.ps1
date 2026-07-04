param(
  [switch]$NativeOnly,
  [switch]$RustOnly,
  [switch]$FrontendOnly
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
$CargoBin = Join-Path $env:USERPROFILE ".cargo/bin"

if (Test-Path $CargoBin) {
  $env:Path = "$CargoBin;$env:Path"
}

function Find-CMake {
  $Command = Get-Command cmake -ErrorAction SilentlyContinue
  if ($Command) {
    return $Command.Source
  }

  $Candidates = @(
    "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
  )

  foreach ($Candidate in $Candidates) {
    if (Test-Path $Candidate) {
      return $Candidate
    }
  }

  throw "cmake.exe was not found. Install Visual Studio Build Tools with C++ CMake tools."
}

function Find-VcVars64 {
  $Candidates = @(
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
    "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
    "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
  )

  foreach ($Candidate in $Candidates) {
    if (Test-Path $Candidate) {
      return $Candidate
    }
  }

  throw "vcvars64.bat was not found. Install Visual Studio Build Tools with Desktop development with C++."
}

if (-not $NativeOnly -and -not $FrontendOnly) {
  Push-Location $RepoRoot
  cargo check --workspace
  Pop-Location
}

if (-not $NativeOnly -and -not $RustOnly) {
  Push-Location (Join-Path $RepoRoot "apps/desktop")
  npm install
  npm run build
  Pop-Location
}

if (-not $RustOnly -and -not $FrontendOnly) {
  $BuildDir = Join-Path $RepoRoot "native/windows-virtual-camera/build-release"
  $CMake = Find-CMake
  $VcVars64 = Find-VcVars64
  $NativeSource = Join-Path $RepoRoot "native/windows-virtual-camera"

  cmd /s /c "call `"$VcVars64`" >nul && `"$CMake`" -S `"$NativeSource`" -B `"$BuildDir`" -G `"NMake Makefiles`" -DCMAKE_BUILD_TYPE=Release && `"$CMake`" --build `"$BuildDir`""
  if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
  }
}
