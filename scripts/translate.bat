@echo off
rem Generates project\hle.cfg and translates the game when the inputs changed. Returns 1 on failure.
rem Expects ROOT, BUILD, GEN, GAME_DOL in the environment and the translator already built.
setlocal EnableExtensions
rem UC_PROJECT_DIR and UC_PROJECT_CFG select the project of another game (default: project\uc.cfg).
if not defined UC_PROJECT_DIR set "UC_PROJECT_DIR=%ROOT%\project"
if not defined UC_PROJECT_CFG set "UC_PROJECT_CFG=uc.cfg"
set "CFG=%UC_PROJECT_DIR%\%UC_PROJECT_CFG%"
set "HLECFG=%UC_PROJECT_DIR%\hle.cfg"
set "TRANSLATOR=%BUILD%\tools\translator\translator.exe"
python -I "%ROOT%\tools\hle_cfg.py" "%UC_PROJECT_DIR%"
if errorlevel 1 goto :hle_failed
set "KEY="
for /f %%h in ('python -I "%ROOT%\tools\translate_key.py"') do set "KEY=%%h"
if not defined KEY goto :hash_failed
if not exist "%GEN%\game_sources.cmake" goto :translate
findstr /x /l /c:"# translation-input: %KEY%" "%GEN%\game_sources.cmake" >nul 2>&1
if not errorlevel 1 goto :cached
rem the translator keeps every earlier generation (~50 MB each); only the one in game_sources.cmake is used
:translate
"%TRANSLATOR%" --dol "%GAME_DOL%" --cfg "%CFG%" --cfg "%HLECFG%" --out "%GEN%" --shard 150 --cache-key "%KEY%" > "%BUILD%\translate.log" 2>&1
if errorlevel 1 goto :translate_failed
type "%BUILD%\translate.log"
python -I "%ROOT%\tools\prune_generations.py" "%GEN%"
exit /b 0
:cached
echo generated\ is up to date
python -I "%ROOT%\tools\prune_generations.py" "%GEN%"
exit /b 0
:hle_failed
echo ERROR: hle.cfg generation failed
exit /b 1
:hash_failed
echo ERROR: could not hash inputs for the translate cache
exit /b 1
:translate_failed
echo ERROR: translation failed
type "%BUILD%\translate.log"
exit /b 1
