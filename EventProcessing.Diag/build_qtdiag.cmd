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
rem projects. If QTDIR is absent, try the conventional Qt 5 MSVC locations.
if not defined QT_ROOT (
    for /f "delims=" %%D in ('dir /b /s /ad "C:\Qt\5.*\msvc2019_64" 2^>nul') do set "QT_ROOT=%%D"
)

if not defined QT_ROOT (
    echo [qtDiag] ERROR: Qt 5 MSVC x64 was not found.
    echo [qtDiag] Set QTDIR to the kit directory, for example:
    echo [qtDiag]   set QTDIR=C:\Qt\5.15.2\msvc2019_64
    echo [qtDiag] Then restart Visual Studio and rebuild qtDiag.
    exit /b 2
)

if not exist "%QT_ROOT%\lib\cmake\Qt5\Qt5Config.cmake" (
    echo [qtDiag] ERROR: "%QT_ROOT%" is not a Qt 5 MSVC kit directory.
    echo [qtDiag] Expected: %QT_ROOT%\lib\cmake\Qt5\Qt5Config.cmake
    exit /b 3
)

echo [qtDiag] Source : %SOURCE%
echo [qtDiag] Build  : %BUILD%
echo [qtDiag] Qt     : %QT_ROOT%

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
