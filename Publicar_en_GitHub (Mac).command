#!/bin/bash
set -e
cd "$(dirname "$0")"
clear
echo "======================================================================"
echo "  ☁️  Publicando XV Bass Pedal v6.0 en Somospopups/Plugin-Bass"
echo "======================================================================"
if [ ! -d .git ]; then
  git init
  git branch -M main
fi
git remote remove origin 2>/dev/null || true
git remote add origin https://github.com/Somospopups/Plugin-Bass.git
git add -A
git commit -m "XV Bass Pedal v6.0 Custom Edition + Cloud Sync + CI Installers" || true
git push -u origin main
echo ""
echo "✅ ¡Publicado con éxito en https://github.com/Somospopups/Plugin-Bass !"
echo "   • Presets en la nube activos en: xv_updates.json"
echo "   • Instaladores Mac (.pkg) y Windows (.exe) compilándose en la pestaña Actions."
read -r -p "Presioná Enter para cerrar..."
