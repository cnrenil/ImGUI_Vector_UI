@echo off
rem Build ImGuiVectorUIDemo with the (relocated, registry-less) VS2022 on this box.
rem Usage:  build.bat [Debug|Release]      (default Debug)
setlocal
set "CFG=%1"
if "%CFG%"=="" set "CFG=Debug"

set "VC=A:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC\14.44.35207"
set "SDK=A:\Program Files (x86)\Windows Kits\10"
set "VER=10.0.26100.0"

rem Supply the unregistered SDK to the compiler directly (also via the prop sheet below).
set "INCLUDE=%VC%\include;%SDK%\Include\%VER%\ucrt;%SDK%\Include\%VER%\shared;%SDK%\Include\%VER%\um;%SDK%\Include\%VER%\winrt;%SDK%\Include\%VER%\cppwinrt"
set "LIB=%VC%\lib\x64;%SDK%\Lib\%VER%\ucrt\x64;%SDK%\Lib\%VER%\um\x64"
set "PATH=%VC%\bin\Hostx64\x64;%SDK%\bin\%VER%\x64;%PATH%"

set "MSB=A:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"
set "PROPS=%~dp0build\msvc-sdk-paths.props"

"%MSB%" "%~dp0ImGUI_Vector_UI.sln" ^
  /p:Configuration=%CFG% /p:Platform=x64 ^
  /p:WindowsTargetPlatformVersion=%VER% ^
  /p:WindowsSDKInstalled=true /p:WindowsSDK_Desktop_Support=true /p:WindowsSDK_UAP_Support=false ^
  /p:ForceImportBeforeCppTargets="%PROPS%" ^
  /m /nologo /verbosity:minimal
endlocal
