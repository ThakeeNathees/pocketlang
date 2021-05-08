@rem ------------------------------------------------------------------------------
@rem  MIT License
@rem ------------------------------------------------------------------------------
@rem  
@rem  Copyright (c) 2021 Thakee Nathees
@rem  
@rem  Permission is hereby granted, free of charge, to any person obtaining a copy
@rem  of this software and associated documentation files (the "Software"), to deal
@rem  in the Software without restriction, including without limitation the rights
@rem  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
@rem  copies of the Software, and to permit persons to whom the Software is
@rem  furnished to do so, subject to the following conditions:
@rem  
@rem  The above copyright notice and this permission notice shall be included in all
@rem  copies or substantial portions of the Software.
@rem  
@rem  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
@rem  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
@rem  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
@rem  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
@rem  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
@rem  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
@rem  SOFTWARE.
@rem ------------------------------------------------------------------------------

@echo off

:: TODO: Usage.
setlocal

:: To set color. (Ref : https://stackoverflow.com/questions/2048509/how-to-echo-with-different-colors-in-the-windows-command-line)
SETLOCAL EnableDelayedExpansion
for /F "tokens=1,2 delims=#" %%a in ('"prompt #$H#$E# & echo on & for %%b in (1) do rem"') do (
  set "DEL=%%a"
)

if "%~1"=="" goto :ARGS_DONE
if "%~1"=="-h" goto :DISPLAY_HELP
if "%~1"=="-d" goto :SET_DLL_BUILD
goto :ARGS_DONE

:DISPLAY_HELP
echo Usage: msvcbuild [opt]
echo   -h  Display the help message
echo   -d  Compile as a dynamic library
goto :END

:SET_DLL_BUILD
set BUILD_DLL=true
goto :ARGS_DONE

:ARGS_DONE

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

:START

if not exist "..\build\" md "..\build\"

set ADDNL_INCLUDE=/I..\src\include
set ADDNL_CPPFLAGS=/EHsc /MDd

if "%BUILD_DLL%" neq "true" goto :COMPILE
set ADDNL_DEFINES=/DMS_DLL /DMS_COMPILE

:COMPILE
call :ColorText 0f "Building porcess started"
echo.

set object_files=
:: Recursively loop all files in '.' matching *.c and compile.
for /r "..\src" %%f in (*.c) do (
	for %%i in ("%%f") do (
		call set "object_files=%%object_files%% ..\build\%%~ni.obj"
		cl /nologo /c %ADDNL_INCLUDE% %ADDNL_DEFINES% %ADDNL_CPPFLAGS% "%%f" /Fo..\build\%%~ni.obj
		if errorlevel 1 goto :FAIL
	)
)

if "%BUILD_DLL%"=="true" goto :LINK_DLL

call :ColorText 0d "..\build\pocket.lib"
echo.
lib /nologo /OUT:..\build\pocket.lib %object_files%
if errorlevel 1 goto :FAIL

call :ColorText 0d "..\build\pocket.exe"
echo.

cl /nologo %ADDNL_CPPFLAGS% %ADDNL_INCLUDE% %object_files%  /Fe..\build\pocket.exe
if errorlevel 1 goto :FAIL

goto :CLEAN

:LINK_DLL
call :ColorText 0d "pocket.dll"
echo.
link /nologo /dll /out:..\build\pocket.dll /implib:..\build\pocket.lib %object_files%

:CLEAN

for %%o in (%object_files%) do (
	del "%%o"
)

echo.
call :ColorText 0a "Compilation Success"
echo.

goto :END
:FAIL
call :ColorText 0c "Build failed. See the error messages."
echo.

:END
endlocal

goto :eof

:: To set color. (Ref : https://stackoverflow.com/questions/2048509/how-to-echo-with-different-colors-in-the-windows-command-line)
:ColorText
echo off
<nul set /p ".=%DEL%" > "%~2"
findstr /v /a:%1 /R "^$" "%~2" nul
del "%~2" > nul 2>&1
goto :eof


