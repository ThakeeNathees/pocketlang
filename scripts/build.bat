
::  Copyright (c) 2020-2021 Thakee Nathees
::  Distributed Under The MIT License

@echo off
Pushd %cd%
cd %~dp0

:: Root directory of the project
set pocket_root=%~dp0..\

:: ----------------------------------------------------------------------------
::                      PARSE COMMAND LINE ARGS
:: ----------------------------------------------------------------------------

set debug_build=true
set shared_lib=false

goto :PARSE_ARGS

:SHIFT_ARG_2
shift
:SHIFT_ARG_1
shift

:PARSE_ARGS
if (%1)==(-h) goto :PRINT_USAGE
if (%1)==(-c) goto :CLEAN
if (%1)==(-r) set debug_build=false&& goto :SHIFT_ARG_1
if (%1)==(-s) set shared_lib=true&& goto :SHIFT_ARG_1
if (%1)==() goto :CHECK_MSVC

echo Invalid argument "%1"

:PRINT_USAGE
echo Usage: call build.bat [options ...]
echo options:
echo   -h  display this message
echo   -r  Compile the release version of pocketlang (default = debug)
echo   -s  Link the pocket as shared library (default = static link).
echo   -c  Clean all compiled/generated intermediate binary.
goto :END

:: ----------------------------------------------------------------------------
::                      INITIALIZE MSVC ENV
:: ----------------------------------------------------------------------------
:CHECK_MSVC

if not defined INCLUDE goto :MSVC_INIT
goto :START

:MSVC_INIT
echo Not running on MSVC prompt, searching for one...

:: Find vswhere
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
	set VSWHERE_PATH="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
) else ( if exist "%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe" (
	set VSWHERE_PATH="%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
) else (
	echo Can't find vswhere.exe
	goto :NO_VS_PROMPT
))

:: Get the VC installation path
%VSWHERE_PATH% -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -latest -property installationPath > _path_temp.txt
set /p VSWHERE_PATH= < _path_temp.txt
del _path_temp.txt
if not exist "%VSWHERE_PATH%" (
	echo Error: can't find VisualStudio installation directory
	goto :NO_VS_PROMPT
)

echo Found at - %VSWHERE_PATH%

:: Initialize VC for X86_64
call "%VSWHERE_PATH%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 goto :NO_VS_PROMPT
echo Initialized MSVC x86_64
goto :START

:NO_VS_PROMPT
echo You must open a "Visual Studio .NET Command Prompt" to run this script
goto :END

:: ----------------------------------------------------------------------------
::                                START
:: ----------------------------------------------------------------------------
:START

set target_dir=
set addnl_cflags=-W3 -GR /FS -EHsc
set addnl_linkflags=/SUBSYSTEM:CONSOLE
set addnl_cdefines=/D_CRT_SECURE_NO_WARNINGS

:: Relative root dir from a single intermediate dir.


if "%debug_build%"=="false" (
	set cflags=%cflags% -O2 -MD /DNDEBUG
	set target_dir=%pocket_root%build\Release\
) else (
	set cflags=%cflags% -MDd -ZI
	set addnl_cdefines=%addnl_cdefines% /DDEBUG
	set target_dir=%pocket_root%build\Debug\
)

if "%shared_lib%"=="true" (
	set addnl_cdefines=%addnl_cdefines% /DPK_DLL /DPK_COMPILE
)

:: Make intermediate folders.	
if not exist %target_dir%bin\ mkdir %target_dir%bin\
if not exist %target_dir%lib\ mkdir %target_dir%lib\
if not exist %target_dir%obj\pocket mkdir %target_dir%obj\pocket\
if not exist %target_dir%obj\cli\ mkdir %target_dir%obj\cli\

:: ----------------------------------------------------------------------------
::                              COMPILE
:: ----------------------------------------------------------------------------
:COMPILE

cd %target_dir%obj\pocket

cl /nologo /c %addnl_cdefines% %addnl_cflags% /I%pocket_root%src\include\ %pocket_root%src\core\*.c %pocket_root%src\libs\*.c
if errorlevel 1 goto :FAIL

:: If compiling shared lib, jump pass the lib/cli binaries.
if "%shared_lib%"=="true" (
  set pklib=%target_dir%bin\pocket.lib
) else (
  set pklib=%target_dir%lib\pocket.lib
)

:: If compiling shared lib, jump pass the lib/cli binaries.
if "%shared_lib%"=="true" goto :SHARED
lib /nologo %addnl_linkflags% /OUT:%pklib% *.obj
goto :SRC_END

:SHARED
link /nologo /dll /out:%target_dir%bin\pocket.dll /implib:%pklib% *.obj

:SRC_END
if errorlevel 1 goto :FAIL

cd %target_dir%obj\cli

cl /nologo /c %addnl_cdefines% %addnl_cflags% /I%pocket_root%src\include\ %pocket_root%cli\*.c
if errorlevel 1 goto :FAIL

cd %target_dir%bin\

:: Compile the cli executable.
cl /nologo %addnl_cdefines% %target_dir%obj\cli\*.obj %pklib% /Fe%target_dir%bin\pocket.exe
if errorlevel 1 goto :FAIL

:: Navigate to the build directory.
cd ..\..\
goto :SUCCESS

:BUILD_DLL


goto :SUCCESS

:CLEAN

if exist "%pocket_root%build/Debug" rmdir /S /Q "%pocket_root%build/Debug"
if exist "%pocket_root%build/Release" rmdir /S /Q "%pocket_root%build/Release"

echo.
echo Files were cleaned.
goto :END

:: ----------------------------------------------------------------------------
::                              END
:: ----------------------------------------------------------------------------

:SUCCESS
echo.
echo Compilation Success
goto :END

:FAIL
popd
endlocal
echo Build failed. See the error messages.
exit /b 1

:END
popd
endlocal
goto :eof


