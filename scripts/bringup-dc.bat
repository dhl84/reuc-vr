@echo off
rem Bring-up build of The Darkside Chronicles (SBDE08) in build-dc\ from project\dc, and a short run.
rem Usage: scripts\bringup-dc.bat [uc.exe arguments]   (default: --seconds 20)
rem DC_INSTALL_DIR (local.env or the shell) is the folder that holds game\DATA\sys\main.dol of your own unpacked SBDE08 disc.
rem The translated code goes to generated-dc\ (git-ignored, never publish it). The translator is the one in build\.
rem DC_NO_RUN=1 builds only. DC_TRACE=OFF builds without the function entry trace.
setlocal EnableExtensions
set "ROOT=%~dp0.."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"
set "BUILD=%ROOT%\build"
set "GEN=%ROOT%\generated-dc"
set "OUT=%ROOT%\build-dc"
call "%~dp0local.bat"
if not defined DC_INSTALL_DIR (echo ERROR: DC_INSTALL_DIR is not set: the folder with game\DATA\sys\main.dol of SBDE08 & exit /b 1)
set "GAME_DOL=%DC_INSTALL_DIR%\game\DATA\sys\main.dol"
if not exist "%GAME_DOL%" (echo ERROR: main.dol not found: %GAME_DOL% & exit /b 1)
set "UC_PROJECT_DIR=%ROOT%\project\dc"
set "UC_PROJECT_CFG=dc.cfg"
if not defined DC_TRACE set "DC_TRACE=ON"
set "ARGS=%*"
if not defined ARGS set "ARGS=--seconds 20"
call "%~dp0env.bat" || exit /b 1
if not exist "%BUILD%" mkdir "%BUILD%"
if not exist "%OUT%" mkdir "%OUT%"
cmake -S "%ROOT%" -B "%BUILD%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DUC_THINLTO=ON > "%BUILD%\configure.log" 2>&1
if errorlevel 1 goto :fail
cmake --build "%BUILD%" --target translator > "%BUILD%\translator_build.log" 2>&1
if errorlevel 1 goto :fail
call "%~dp0translate.bat" || exit /b 1
cmake -S "%ROOT%" -B "%OUT%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DUC_TITLE=dc -DUC_GENERATED_DIR="%GEN%" -DUC_TRACE=%DC_TRACE% > "%OUT%\configure.log" 2>&1
if errorlevel 1 goto :fail
cmake --build "%OUT%" --target uc > "%OUT%\build.log" 2>&1
if errorlevel 1 goto :fail
if defined DC_NO_RUN (echo built %OUT%\game\uc.exe & exit /b 0)
"%OUT%\game\uc.exe" --game "%DC_INSTALL_DIR%\game" %ARGS% 2> "%OUT%\run.log"
set "RC=%errorlevel%"
echo uc.exe exit code %RC%
powershell -NoProfile -Command "Get-Content -LiteralPath ($env:OUT + '\run.log') -Tail 60"
exit /b %RC%
:fail
echo ERROR: build failed, see the logs in %BUILD% and %OUT%
powershell -NoProfile -Command "Get-Content -LiteralPath ($env:OUT + '\build.log') -Tail 40 -ErrorAction SilentlyContinue"
exit /b 1
