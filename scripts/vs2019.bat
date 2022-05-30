@echo off

pushd %cd%
cd %~dp0

:: unzip.exe comes with git for windows.

if not exist premake5.exe (
  echo premake5.exe not exists, downloading
  echo.
  curl -L https://github.com/premake/premake-core/releases/download/v5.0.0-beta1/premake-5.0.0-beta1-windows.zip --output premake5.zip
  unzip premake5.zip && premake5.zip
  echo.
)

premake5 vs2019

popd
