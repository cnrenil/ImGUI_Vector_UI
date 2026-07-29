[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'Release',

    [string]$BuildDirectory = '',
    [string]$InstallPrefix = '',

    [switch]$Clean,
    [switch]$NoDemo,
    [switch]$NoTests,
    [switch]$Vulkan,
    [switch]$VerifyPackage,
    [switch]$ComponentOnly,

    [string]$VisualStudioRoot = 'A:\Program Files\Microsoft Visual Studio\2022\Professional',
    [string]$MsvcVersion = '14.44.35207',
    [string]$WindowsSdkRoot = 'A:\Program Files (x86)\Windows Kits\10',
    [string]$WindowsSdkVersion = '10.0.26100.0',

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$CMakeArguments
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Assert-Path {
    param([string]$Path, [string]$Description)
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Description was not found: $Path"
    }
}

function Invoke-Native {
    param([string]$FilePath, [string[]]$Arguments)
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $FilePath $($Arguments -join ' ')"
    }
}

$RepositoryRoot = $PSScriptRoot
$SourceDirectory = if ($ComponentOnly) {
    Join-Path $RepositoryRoot 'components\ImGuiVectorUI'
} else {
    $RepositoryRoot
}
if ($VerifyPackage -and [string]::IsNullOrWhiteSpace($InstallPrefix)) {
    throw '-VerifyPackage requires -InstallPrefix.'
}
if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $BuildDirectory = Join-Path $RepositoryRoot "out\cmake-$($Configuration.ToLowerInvariant())"
} elseif (-not [System.IO.Path]::IsPathRooted($BuildDirectory)) {
    $BuildDirectory = Join-Path $RepositoryRoot $BuildDirectory
}

$MsvcRoot = Join-Path $VisualStudioRoot "VC\Tools\MSVC\$MsvcVersion"
$SdkIncludeRoot = Join-Path $WindowsSdkRoot "Include\$WindowsSdkVersion"
$SdkLibraryRoot = Join-Path $WindowsSdkRoot "Lib\$WindowsSdkVersion"
$SdkBinaryRoot = Join-Path $WindowsSdkRoot "bin\$WindowsSdkVersion\x64"
$MsvcBinaryRoot = Join-Path $MsvcRoot 'bin\Hostx64\x64'
$CMake = Join-Path $VisualStudioRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$CTest = Join-Path $VisualStudioRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
$NMake = Join-Path $MsvcBinaryRoot 'nmake.exe'

Assert-Path $RepositoryRoot 'Repository root'
Assert-Path (Join-Path $SourceDirectory 'CMakeLists.txt') 'CMake project'
Assert-Path $CMake 'Visual Studio CMake'
Assert-Path $CTest 'Visual Studio CTest'
Assert-Path $NMake 'NMake'
Assert-Path (Join-Path $MsvcBinaryRoot 'cl.exe') 'MSVC compiler'
Assert-Path (Join-Path $SdkBinaryRoot 'rc.exe') 'Windows SDK resource compiler'
Assert-Path (Join-Path $SdkBinaryRoot 'mt.exe') 'Windows SDK manifest tool'
Assert-Path (Join-Path $SdkIncludeRoot 'um\Windows.h') 'Windows SDK headers'
Assert-Path (Join-Path $SdkLibraryRoot 'um\x64\kernel32.lib') 'Windows SDK libraries'

$IncludePaths = @(
    (Join-Path $MsvcRoot 'include'),
    (Join-Path $SdkIncludeRoot 'ucrt'),
    (Join-Path $SdkIncludeRoot 'shared'),
    (Join-Path $SdkIncludeRoot 'um'),
    (Join-Path $SdkIncludeRoot 'winrt'),
    (Join-Path $SdkIncludeRoot 'cppwinrt')
)
$LibraryPaths = @(
    (Join-Path $MsvcRoot 'lib\x64'),
    (Join-Path $SdkLibraryRoot 'ucrt\x64'),
    (Join-Path $SdkLibraryRoot 'um\x64')
)

$env:PATH = "$MsvcBinaryRoot;$SdkBinaryRoot;$($env:PATH)"
$env:INCLUDE = $IncludePaths -join ';'
$env:LIB = $LibraryPaths -join ';'
$env:CMAKE_GENERATOR = 'NMake Makefiles'
# Keep /showIncludes output stable. CMake parses and removes this dependency
# stream; localized MSVC output can be decoded with a mismatched console code page.
$env:VSLANG = '1033'

$BuildDemo = if ($NoDemo -or $ComponentOnly) { 'OFF' } else { 'ON' }
$BuildTests = if ($NoTests) { 'OFF' } else { 'ON' }
$BuildVulkan = if ($Vulkan) { 'ON' } else { 'OFF' }
$CMakeNMake = $NMake.Replace('\', '/')
$CMakeCompiler = (Join-Path $MsvcBinaryRoot 'cl.exe').Replace('\', '/')
$CMakeRc = (Join-Path $SdkBinaryRoot 'rc.exe').Replace('\', '/')
$CMakeMt = (Join-Path $SdkBinaryRoot 'mt.exe').Replace('\', '/')

# This SDK/VS installation is not registered. Explicit compiler/tool paths keep
# CMake from consulting the missing registry entries during compiler detection.
$ConfigureArguments = @(
    '-S', $SourceDirectory,
    '-B', $BuildDirectory,
    '-G', 'NMake Makefiles',
    "-DCMAKE_BUILD_TYPE=$Configuration",
    "-DCMAKE_MAKE_PROGRAM=$CMakeNMake",
    "-DCMAKE_CXX_COMPILER=$CMakeCompiler",
    "-DCMAKE_RC_COMPILER=$CMakeRc",
    "-DCMAKE_MT=$CMakeMt",
    "-DIVUI_IMGUI_SOURCE_DIR=$($RepositoryRoot.Replace('\', '/'))/third_party/imgui",
    '-DIVUI_BUILD_WIN32_PLATFORM=ON',
    '-DIVUI_BUILD_DX11_EFFECTS=ON',
    '-DIVUI_BUILD_DX12_EFFECTS=ON',
    "-DIVUI_BUILD_VULKAN_EFFECTS=$BuildVulkan",
    "-DIVUI_BUILD_TESTS=$BuildTests"
)

if (-not $ComponentOnly) {
    $ConfigureArguments += "-DIVUI_BUILD_DEMO=$BuildDemo"
}

if ($CMakeArguments) {
    $ConfigureArguments += $CMakeArguments
}

if ($Vulkan) {
    if ([string]::IsNullOrWhiteSpace($env:VULKAN_SDK)) {
        throw 'Vulkan was requested, but VULKAN_SDK is not set. Install the Vulkan SDK or omit -Vulkan.'
    }
    Assert-Path (Join-Path $env:VULKAN_SDK 'Include\vulkan\vulkan.h') 'Vulkan headers'
    Assert-Path (Join-Path $env:VULKAN_SDK 'Bin\glslangValidator.exe') 'glslangValidator'
    $env:PATH = "$(Join-Path $env:VULKAN_SDK 'Bin');$($env:PATH)"
}

if ($Clean -and (Test-Path -LiteralPath $BuildDirectory)) {
    Remove-Item -LiteralPath $BuildDirectory -Recurse -Force
}

Write-Host "Configuring ImGuiVectorUI ($Configuration)"
Write-Host "  Build: $BuildDirectory"
Write-Host "  MSVC:  $MsvcRoot"
Write-Host "  SDK:   $WindowsSdkRoot ($WindowsSdkVersion)"
Invoke-Native $CMake $ConfigureArguments

Write-Host 'Building ImGuiVectorUI'
Invoke-Native $CMake @('--build', $BuildDirectory)

if (-not $NoTests) {
    # The CRT is not registered on this machine either. Add the matching local
    # runtime directory for CTest without changing the library's CRT policy.
    $RedistRoot = Join-Path $VisualStudioRoot 'VC\Redist\MSVC'
    if (Test-Path -LiteralPath $RedistRoot) {
        $CrtSuffix = if ($Configuration -eq 'Debug') {
            'debug_nonredist\x64\Microsoft.VC143.DebugCRT'
        } else {
            'x64\Microsoft.VC143.CRT'
        }
        $Crt = Get-ChildItem -LiteralPath $RedistRoot -Directory |
            Sort-Object Name -Descending |
            ForEach-Object { Join-Path $_.FullName $CrtSuffix } |
            Where-Object { Test-Path -LiteralPath $_ } |
            Select-Object -First 1
        if ($Crt) {
            $env:PATH = "$Crt;$($env:PATH)"
        }
    }
    if ($Configuration -eq 'Debug') {
        $DebugUcrt = Join-Path $SdkBinaryRoot 'ucrt'
        if (Test-Path -LiteralPath $DebugUcrt) {
            $env:PATH = "$DebugUcrt;$($env:PATH)"
        }
    }
    Write-Host 'Running tests'
    Invoke-Native $CTest @('--test-dir', $BuildDirectory, '--output-on-failure')
}

if (-not [string]::IsNullOrWhiteSpace($InstallPrefix)) {
    if (-not [System.IO.Path]::IsPathRooted($InstallPrefix)) {
        $InstallPrefix = Join-Path $RepositoryRoot $InstallPrefix
    }
    Write-Host "Installing to $InstallPrefix"
    Invoke-Native $CMake @('--install', $BuildDirectory, '--prefix', $InstallPrefix)
}

if ($VerifyPackage) {
    $ConsumerSource = Join-Path $RepositoryRoot 'tests\package-consumer'
    $ConsumerBuild = "$BuildDirectory-package-consumer"
    $PackageDirectory = Join-Path $InstallPrefix 'lib\cmake\ImGuiVectorUI'
    Assert-Path (Join-Path $ConsumerSource 'CMakeLists.txt') 'Package consumer'
    Assert-Path (Join-Path $PackageDirectory 'ImGuiVectorUIConfig.cmake') 'Installed package'

    if ($Clean -and (Test-Path -LiteralPath $ConsumerBuild)) {
        Remove-Item -LiteralPath $ConsumerBuild -Recurse -Force
    }

    $ConsumerArguments = @(
        '-S', $ConsumerSource,
        '-B', $ConsumerBuild,
        '-G', 'NMake Makefiles',
        "-DCMAKE_BUILD_TYPE=$Configuration",
        "-DCMAKE_MAKE_PROGRAM=$CMakeNMake",
        "-DCMAKE_CXX_COMPILER=$CMakeCompiler",
        "-DCMAKE_RC_COMPILER=$CMakeRc",
        "-DCMAKE_MT=$CMakeMt",
        "-DImGuiVectorUI_DIR=$($PackageDirectory.Replace('\', '/'))"
    )
    Write-Host 'Verifying installed package consumer'
    Invoke-Native $CMake $ConsumerArguments
    Invoke-Native $CMake @('--build', $ConsumerBuild)
}

Write-Host 'Build completed successfully.'
