@echo off
setlocal

set MSYSROOT=C:\msys64
set MSYSTEM=MINGW32
set CHERE_INVOKING=1
set LLVM_PATH=C:/Program Files/LLVM/RELEASE_LIBS/bin

for /f "delims=" %%i in ('where python') do set "PYTHON_PATH=%%~dpi" && goto foundpython
goto nopython

:foundpython
:: Fixup.
set PYTHON_PATH=%PYTHON_PATH:\=/%

echo Python: %PYTHON_PATH%

for /f "delims=" %%i in ('where dexcc') do set "DEXTR_PATH=%%~dpi" && goto founddex
goto nodex

:founddex
:: Fixup.
set DEXTR_PATH=%DEXTR_PATH:\=/%
echo dex: %DEXTR_PATH%



:: start %MSYSROOT%\msys2_shell.cmd -defterm -no-start -mingw32 -c "export PATH=""$(cygpath -u ""%PYTHON_PATH%""):$(cygpath -u ""%LLVM_PATH%""):$(cygpath -u ""%DEXTR_PATH%""):$PATH""; exec bash"
start %MSYSROOT%\msys2_shell.cmd -defterm -no-start -mingw32 -c "export PATH=""$(cygpath -u ""%LLVM_PATH%""):$(cygpath -u ""%DEXTR_PATH%""):$PATH""; exec bash"
:: assumes we have mingw-w64-i686-llvm, mingw-w64-i686-clang, mingw-w64-i686-python
:: mingw-w64-i686-llvm-tools?
@REM start %MSYSROOT%\msys2_shell.cmd -defterm -no-start -mingw32 -c "export PATH=""$(cygpath -u ""%DEXTR_PATH%""):$PATH""; exec bash"
goto done

:nopython
echo Couldn't find Python installation!
goto done

:nodex
echo Couldn't find Dextr installation! Make sure it's present in your PATH!
goto done

:done