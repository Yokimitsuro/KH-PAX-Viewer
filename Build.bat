@echo off
rem Builds KH PAX Viewer (Release) into build\Release\ with Visual Studio's C++ tools and CMake.
rem Needs Visual Studio 2022 or later with "Desktop development with C++" (it brings CMake and Ninja).
setlocal
set "HERE=%~dp0"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo Visual Studio was not found: install it with "Desktop development with C++".
    exit /b 1
)
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS=%%i"
if not defined VS (
    echo Visual Studio's C++ tools were not found.
    exit /b 1
)
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cmake -S "%HERE%." -B "%HERE%build" -G Ninja -DCMAKE_BUILD_TYPE=Release || exit /b 1
cmake --build "%HERE%build" || exit /b 1
ctest --test-dir "%HERE%build" --output-on-failure || exit /b 1
if not exist "%HERE%build\Release" mkdir "%HERE%build\Release"
copy /y "%HERE%build\KH PAX Viewer.exe" "%HERE%build\Release\" >nul || (
    echo Could not copy into build\Release: close KH PAX Viewer if it is running, then build again.
    exit /b 1
)
echo.
echo Built: "%HERE%build\Release\KH PAX Viewer.exe"
endlocal
