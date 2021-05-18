@echo off

:: Resolve path hasn't implemented.
cd lang
echo Testing "import.pk"
pocket import.pk
if  %errorlevel% neq 0 goto :FAILED
cd ..

set files=(            ^
  lang\basics.pk       ^
  lang\if.pk           ^
  lang\logical.pk      ^
  lang\chain_call.pk   ^
                       ^
  examples\fib.pk      ^
  examples\prime.pk    ^
)

set errorlevel=0

for %%f in %files% do (
  echo Testing %%f
  pocket %%f
  if  %errorlevel% neq 0 goto :FAILED
)
goto :SUCCESS

:FAILED
echo.
echo Test failed.
goto :END

:SUCCESS
echo.
echo All tests were passed.
goto :END

:END
