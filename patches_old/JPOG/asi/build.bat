@echo off
setlocal

:: Find Visual Studio via vswhere
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -find VC\Auxiliary\Build\vcvarsall.bat 2^>nul`) do set "VCVARSALL=%%i"
if not defined VCVARSALL (
    echo ERROR: Cannot find vcvarsall.bat. Install Visual Studio with C++ tools.
    exit /b 1
)

echo Setting up x86 build environment...
call "%VCVARSALL%" x86 >nul 2>&1

echo Compiling jpog_view_fix.c...
cl /nologo /W3 /O2 /GS- /Zl /c jpog_view_fix.c
if errorlevel 1 goto fail

echo Linking jpog_view_fix.asi...
link /nologo /DLL /NODEFAULTLIB /ENTRY:DllMain /OUT:jpog_view_fix.asi ^
     jpog_view_fix.obj kernel32.lib
if errorlevel 1 goto fail

echo.
echo === Build successful: jpog_view_fix.asi ===
echo Deploy: copy jpog_view_fix.asi to your game's plugins\ folder.
goto end

:fail
echo === BUILD FAILED ===
exit /b 1

:end
del /q *.obj *.exp *.lib 2>nul
endlocal
