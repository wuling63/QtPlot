# build.ps1 — 設定好 MSVC 環境後建置 QtPlot。
#
#   powershell -ExecutionPolicy Bypass -File build.ps1            # Release 建置
#   powershell -ExecutionPolicy Bypass -File build.ps1 -Run       # 建置後開啟 Cookbook
#   powershell -ExecutionPolicy Bypass -File build.ps1 -Export o  # 建置後把範例存成 PNG
#   powershell -ExecutionPolicy Bypass -File build.ps1 -NoTest    # 跳過建置後的自我檢查
#
# 預設建置完會跑 ctest（數值、換圖、C 介面、C#、互動，約 10 秒），沒過就不往下執行。
param(
    [string]$Config = "Release",
    [string]$Qt = "D:/Qt/6.11.1/msvc2022_64",
    [switch]$Clean,
    # vswhere 的版本範圍。Qt 是 msvc2022_64，所以預設鎖在 VS2022。
    [string]$VsRange = "[17.0,18.0)",
    [switch]$Run,
    [switch]$NoTest,
    [string]$Export = ""
)

$ErrorActionPreference = "Stop"
$root  = $PSScriptRoot
$build = Join-Path $root "build"

# vcvars64 的位置：先問 vswhere，問不到再用常見路徑。
#
# 這裡刻意限定 VS2022（17.x）而不是單純的 -latest：Qt 用的是 msvc2022_64 這套
# 預編譯二進位，機器上若另外裝了更新版的 Visual Studio，-latest 會挑到它，
# 於是 build 目錄裡釘住的舊 cl.exe 配上新版的 STL 標頭 —— 會直接撞上
# 'Unexpected compiler version' 之類的錯誤。
# 真的要換整套工具鏈，請改 -VsRange 並且搭配 -Clean 重新設定。
$vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
$vcvars = $null
if (Test-Path $vswhere) {
    $vsPath = & $vswhere -version $VsRange -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($vsPath) { $vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat" }
}
if (-not $vcvars -or -not (Test-Path $vcvars)) {
    throw "在 $VsRange 範圍內找不到 vcvars64.bat。請確認已安裝 Visual Studio 2022 的 C++ 工作負載，或自行指定 -VsRange。"
}

Write-Host "工具鏈: $vsPath" -ForegroundColor DarkGray

$cmake = "D:\Qt\Tools\CMake_64\bin\cmake.exe"
if (-not (Test-Path $cmake)) { $cmake = "cmake" }
# ctest 跟 cmake 放在同一個目錄
$ctest = if ($cmake -eq "cmake") { "ctest" } else { Join-Path (Split-Path $cmake) "ctest.exe" }

if ($Clean -and (Test-Path $build)) {
    Write-Host "清掉舊的建置目錄…" -ForegroundColor Yellow
    Remove-Item -Recurse -Force $build
}

$cfgCmd = "`"$cmake`" -S `"$root`" -B `"$build`" -G Ninja -DCMAKE_BUILD_TYPE=$Config -DCMAKE_PREFIX_PATH=`"$Qt`""
$bldCmd = "`"$cmake`" --build `"$build`""

Write-Host "設定…" -ForegroundColor Cyan
cmd /c "`"$vcvars`" >nul 2>&1 && $cfgCmd"
if ($LASTEXITCODE -ne 0) { throw "CMake 設定失敗" }

Write-Host "建置…" -ForegroundColor Cyan
cmd /c "`"$vcvars`" >nul 2>&1 && $bldCmd"
if ($LASTEXITCODE -ne 0) { throw "建置失敗" }

$exe = Join-Path $build "bin\QtPlotDemo.exe"
Write-Host "完成: $exe" -ForegroundColor Green

# 執行時要找得到 Qt 的 DLL
$env:PATH = "$($Qt -replace '/','\')\bin;$env:PATH"

if (-not $NoTest) {
    Write-Host "自我檢查…" -ForegroundColor Cyan
    & $ctest --test-dir $build --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "自我檢查沒有通過（用 -NoTest 可以先跳過）" }
}

if ($Export) {
    & $exe --export $Export
} elseif ($Run) {
    & $exe
}
