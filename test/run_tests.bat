@echo off

set files=(         ^
  lang\basics.pk    ^
  lang\import.pk    ^
  lang\if.pk        ^
  lang\logical.pk   ^
                    ^
  examples\fib.pk   ^
  examples\prime.pk ^
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
