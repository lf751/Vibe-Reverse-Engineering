@echo off
setlocal

if not defined REMIX_RUNTIME_SOURCE set "REMIX_RUNTIME_SOURCE=%~dp0..\..\..\..\dxvk-remix_numos"
if not exist "%REMIX_RUNTIME_SOURCE%\public\include\remix\remix_c.h" (
    echo ERROR: REMIX_RUNTIME_SOURCE must contain public\include\remix\remix_c.h.
    exit /b 1
)

:: Find Visual Studio via vswhere
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -find VC\Auxiliary\Build\vcvarsall.bat 2^>nul`) do set "VCVARSALL=%%i"
if not defined VCVARSALL (
    echo ERROR: Cannot find vcvarsall.bat. Install Visual Studio with C++ tools.
    exit /b 1
)

echo Setting up x86 build environment...
call "%VCVARSALL%" x86 >nul 2>&1

echo Compiling jpog_view_fix.c...
cl /nologo /W3 /O2 /Oi /Gy /GL /GS- /Zl /I"%REMIX_RUNTIME_SOURCE%\public\include" /c jpog_view_fix.c terrain_water_reset.c
if errorlevel 1 goto fail

echo Linking jpog_view_fix.asi...
link /nologo /DLL /LTCG /OPT:REF /OPT:ICF /NODEFAULTLIB /ENTRY:DllMain /OUT:jpog_view_fix.asi ^
     jpog_view_fix.obj terrain_water_reset.obj kernel32.lib
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
