@echo off
setlocal
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set "OPENMPT_SOLUTION=third_party\libopenmpt-0.8.7-source\build\vs2022win10\libopenmpt-small.sln"
set "OPENMPT_LIBRARY=third_party\libopenmpt-0.8.7-source\build\lib\vs2022win10\x86_64\Release\libopenmpt-small.lib"
if not exist "%VCVARS%" (
  echo Visual Studio C++ Build Tools were not found.
  exit /b 1
)
set "Path="
call "%VCVARS%" >nul
if errorlevel 1 exit /b %errorlevel%
if not exist "%OPENMPT_LIBRARY%" (
  msbuild "%OPENMPT_SOLUTION%" /m /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145 /p:SpectreMitigation=false /v:minimal
  if errorlevel 1 exit /b %errorlevel%
)
cmake -S . -B build-v92-nmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b %errorlevel%
cmake --build build-v92-nmake
if errorlevel 1 exit /b %errorlevel%
if not exist build-v92-release mkdir build-v92-release
copy /Y build-v92-nmake\audiocommander_v92.exe audiocommander_v92.exe >nul
copy /Y build-v92-nmake\audiocommander_v92.exe build-v92-release\audiocommander_v92.exe >nul
if not exist build-v92-release\skins mkdir build-v92-release\skins
copy /Y skins\*.skn build-v92-release\skins\ >nul
for %%F in (avcodec-63.dll avformat-63.dll avutil-61.dll swresample-7.dll) do (
  copy /Y "third_party\ffmpeg-minimal-pack\bin\%%F" "build-v92-nmake\%%F" >nul
  copy /Y "third_party\ffmpeg-minimal-pack\bin\%%F" "build-v92-release\%%F" >nul
)
echo Built %CD%\audiocommander_v92.exe
