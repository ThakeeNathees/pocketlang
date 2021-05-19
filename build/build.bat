
@echo off

:: To set color. (Ref : https://stackoverflow.com/questions/2048509/how-to-echo-with-different-colors-in-the-windows-command-line)
SETLOCAL EnableDelayedExpansion
for /F "tokens=1,2 delims=#" %%a in ('"prompt #$H#$E# & echo on & for %%b in (1) do rem"') do (
  set "DEL=%%a"
)

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
if (%1)==(-clean) goto :CLEAN
if (%1)==(-d) set debug_build=%2&& goto :SHIFT_ARG_2
if (%1)==(-s) set shared_lib=%~2&& goto :SHIFT_ARG_2
if (%1)==() goto :CHECK_MSVC

:PRINT_USAGE
echo Usage: call build.bat [options ...]
echo options:
echo   -d trut/false   Set false to build release binary. Default is true.
echo   -s true/false   Set true to build shared/dynamic library. Default is false.
echo   -clean          Clean all compiled/generated intermediate binary.
goto :END

:: ----------------------------------------------------------------------------
::                      INITIALIZE MSVC ENV
:: ----------------------------------------------------------------------------
:CHECK_MSVC

if not defined INCLUDE goto :MSVC_INIT
goto :START

:MSVC_INIT
call :ColorText 0f "Not running on MSVM prompt, searching for one..."
echo.

:: Find vswhere
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
	set VSWHERE_PATH="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
) else ( if exist "%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe" (
	set VSWHERE_PATH="%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
) else (
	call :ColorText 0c "Can't find vswhere.exe"
	echo.
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

call :ColorText 0f "Found at - "
echo %VSWHERE_PATH%

:: Initialize VC for X86_64
call "%VSWHERE_PATH%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 goto :NO_VS_PROMPT
call :ColorText 0f "Initialized MSVC x86_64"
echo. 
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
set root_dir=..\..\..\

if "%debug_build%"=="false" (
	set cflags=%cflags% -O2 -MD
	set target_dir=release\
) else (
	set cflags=%cflags% -MDd -ZI
	set addnl_cdefines=%addnl_cdefines% /DDEBUG
	set target_dir=debug\
)

if "%shared_lib%"=="true" (
	set addnl_cdefines=%addnl_cdefines% /DPK_DLL /DPK_COMPILE
)

:: Make intermediate folders.	
if not exist %target_dir%src\ md %target_dir%src\
if not exist %target_dir%cli\ md %target_dir%cli\
if not exist %target_dir%bin\ md %target_dir%bin\

:: ----------------------------------------------------------------------------
::                              COMPILE
:: ----------------------------------------------------------------------------
:COMPILE

:: Go inside src\ build all src files.
cd %target_dir%src\
cl /nologo /c %addnl_cdefines% %addnl_cflags% %root_dir%src\*.c
if errorlevel 1 goto :FAIL

:: If compiling shared lib, jump pass the lib/cli binaries.
if "%shared_lib%"=="true" goto :BUILD_DLL

lib /nologo %addnl_linkflags% /OUT:..\bin\pocket.lib *.obj
if errorlevel 1 goto :FAIL

:: Go inside cli\ from src\ build all cli files.
cd ..\cli\
cl /nologo /c %addnl_cdefines% %addnl_cflags% /I%root_dir%src\include\ %root_dir%cli\*.c
if errorlevel 1 goto :FAIL

:: Compile the cli executable.
cl /nologo %addnl_cdefines% *.obj ..\bin\pocket.lib /Fe..\bin\pocket.exe
if errorlevel 1 goto :FAIL

:: Navigate to the build directory.
cd ..\..\
goto :SUCCESS

:BUILD_DLL
link /nologo /dll /out:..\bin\pocket.dll /implib:..\bin\pocket-dll.lib *.obj
if errorlevel 1 goto :FAIL
cd ..\..\
goto :SUCCESS

:CLEAN

if exist "debug" rmdir /S /Q "debug"
if exist "release" rmdir /S /Q "release"

echo.
call :ColorText 0a "Files were cleaned."
echo.
goto :END

:: ----------------------------------------------------------------------------
::                              END
:: ----------------------------------------------------------------------------

:SUCCESS
echo.
call :ColorText 0a "Compilation Success"
echo.
goto :END

:FAIL
call :ColorText 0c "Build failed. See the error messages."
echo.
goto :END

:END
endlocal
goto :eof
:: ----------------------------------------------------------------------------
::                      PRINT COLORED MESSAGE
:: ----------------------------------------------------------------------------

:: Reference - https://stackoverflow.com/questions/2048509/how-to-echo-with-different-colors-in-the-windows-command-line)
:ColorText
echo off
<nul set /p ".=%DEL%" > "%~2"
findstr /v /a:%1 /R "^$" "%~2" nul
del "%~2" > nul 2>&1
goto :eof

