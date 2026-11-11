@echo off
cd /d "%~dp0"
echo ======================================================================
echo   Publicando XV Bass Pedal v6.0 en Somospopups/Plugin-Bass
echo ======================================================================
if not exist .git (
  git init
  git branch -M main
)
git remote remove origin 2>nul
git remote add origin https://github.com/Somospopups/Plugin-Bass.git
git add -A
git commit -m "XV Bass Pedal v6.0 Custom Edition + Cloud Sync + CI Installers"
git push -u origin main
echo.
echo Listo! Publicado en https://github.com/Somospopups/Plugin-Bass
pause
