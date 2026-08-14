@echo off
setlocal EnableExtensions

set "CONFIG=%~1"
set "ACTION=%~2"
set "QT_ROOT=%~3"
set "SOURCE=%~dp0.."

if not defined CONFIG set "CONFIG=Release"
if not defined ACTION set "ACTION=build"
if not defined QT_ROOT set "QT_ROOT=%QTDIR%"
set "BUILD=%SOURCE%\build\vs-%CONFIG%"

rem Qt VS Tools does not always export its selected Qt version to Makefile
rem projects. If QTDIR is absent, try supported Qt 5/6 MSVC locations.
if not defined QT_ROOT (
    for /d %%V in ("C:\Qt\5.12*" "C:\Qt\5.13*" "C:\Qt\5.14*" "C:\Qt\5.15*") do (
        if exist "%%~fV\msvc2019_64\lib\cmake\Qt5\Qt5Config.cmake" set "QT_ROOT=%%~fV\msvc2019_64"
        if exist "%%~fV\msvc2017_64\lib\cmake\Qt5\Qt5Config.cmake" set "QT_ROOT=%%~fV\msvc2017_64"
    )
    for /d %%V in ("C:\Qt\6.*") do (
        if exist "%%~fV\msvc2019_64\lib\cmake\Qt6\Qt6Config.cmake" set "QT_ROOT=%%~fV\msvc2019_64"
        if exist "%%~fV\msvc2022_64\lib\cmake\Qt6\Qt6Config.cmake" set "QT_ROOT=%%~fV\msvc2022_64"
    )
)

if not defined QT_ROOT (
    echo [qtDiag] ERROR: A supported Qt MSVC x64 kit was not found.
    echo [qtDiag] Install Qt 5.12+ or Qt 6, such as Qt 6.5.3 msvc2019_64.
    echo [qtDiag] Set QTDIR to the kit directory, for example:
    echo [qtDiag]   set QTDIR=C:\Qt\5.15.2\msvc2019_64
    echo [qtDiag] Then restart Visual Studio and rebuild qtDiag.
    exit /b 2
)

if not exist "%QT_ROOT%\lib\cmake\Qt5\Qt5Config.cmake" if not exist "%QT_ROOT%\lib\cmake\Qt6\Qt6Config.cmake" (
    echo [qtDiag] ERROR: "%QT_ROOT%" is not a supported Qt MSVC kit directory.
    echo [qtDiag] Expected Qt5Config.cmake or Qt6Config.cmake below %QT_ROOT%\lib\cmake.
    exit /b 3
)

for /f "delims=" %%V in ('"%QT_ROOT%\bin\qmake.exe" -query QT_VERSION 2^>nul') do set "QT_VERSION=%%V"
if not defined QT_VERSION (
    echo [qtDiag] ERROR: qmake could not determine the Qt version in "%QT_ROOT%".
    exit /b 4
)
for /f "tokens=1,2 delims=." %%A in ("%QT_VERSION%") do (
    set "QT_MAJOR=%%A"
    set "QT_MINOR=%%B"
)
if "%QT_MAJOR%"=="6" goto qt_version_ok
if not "%QT_MAJOR%"=="5" goto unsupported_qt
if %QT_MINOR% LSS 12 goto unsupported_qt
goto qt_version_ok

:unsupported_qt
echo [qtDiag] ERROR: Qt %QT_VERSION% is unsupported. Qt 5.12+ or Qt 6 is required.
exit /b 5

:qt_version_ok

echo [qtDiag] Source : %SOURCE%
echo [qtDiag] Build  : %BUILD%
echo [qtDiag] Qt     : %QT_ROOT%
echo [qtDiag] Version: %QT_VERSION%

if /i "%ACTION%"=="clean" (
    if exist "%BUILD%\CMakeCache.txt" cmake --build "%BUILD%" --config %CONFIG% --target clean
    if errorlevel 1 exit /b 1
    exit /b 0
)

cmake -S "%SOURCE%" -B "%BUILD%" -A x64 -DCMAKE_PREFIX_PATH="%QT_ROOT%"
if errorlevel 1 exit /b %ERRORLEVEL%

if /i "%ACTION%"=="rebuild" (
    cmake --build "%BUILD%" --config %CONFIG% --target CEventProcessingDiagDlg --clean-first
) else (
    cmake --build "%BUILD%" --config %CONFIG% --target CEventProcessingDiagDlg
)
if errorlevel 1 exit /b 1
exit /b 0
