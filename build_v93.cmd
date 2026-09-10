@echo off
setlocal
cd /d "%~dp0"

set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set "CMAKE=C:\Program Files\CMake\bin\cmake.exe"
set "NMAKE=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\nmake.exe"
if not exist "%VCVARS%" (
  echo Visual Studio C++ Build Tools were not found.
  exit /b 1
)
if not exist "%CMAKE%" (
  echo CMake was not found.
  exit /b 1
)
if not exist "%NMAKE%" (
  echo NMake was not found.
  exit /b 1
)
call "%VCVARS%" >nul
if errorlevel 1 exit /b %errorlevel%

"%CMAKE%" -S . -B build-v93-nmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM="%NMAKE%"
if errorlevel 1 exit /b %errorlevel%
"%CMAKE%" --build build-v93-nmake
if errorlevel 1 exit /b %errorlevel%

copy /Y build-v93-nmake\audiocommander_v93.exe audiocommander_v93.exe >nul
if errorlevel 1 exit /b %errorlevel%
for %%F in (avcodec-63.dll avformat-63.dll avutil-61.dll swresample-7.dll libopenmpt.dll openmpt-mpg123.dll openmpt-ogg.dll openmpt-vorbis.dll openmpt-zlib.dll) do (
  copy /Y "build-v93-nmake\%%F" "%%F" >nul
  if errorlevel 1 exit /b %errorlevel%
)
echo Built %CD%\audiocommander_v93.exe
