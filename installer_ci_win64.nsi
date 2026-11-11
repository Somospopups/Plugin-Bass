Unicode True
!include "MUI2.nsh"

Name "XV Bass Pedal v6.0 (Custom Edition + Cloud Sync + Blackbox)"
OutFile "Instalar_XV_Bass_Pedal_Windows_x64.exe"
InstallDir "$PROGRAMFILES64\XV Audio\XV Bass Pedal"
RequestExecutionLevel admin

!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\XV Bass Pedal.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Abrir XV Bass Pedal v6.0 (App Standalone con Afinador y Sync Nube)"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "Spanish"

Section "Plugin VST3 (64-bit para DAWs)" SecVST3
  SectionIn RO
  SetOutPath "$COMMONFILES64\VST3"
  File /r "build\XVBassPedal_artefacts\Release\VST3\XV Bass Pedal.vst3"
SectionEnd

Section "App Independiente Standalone (.exe)" SecStandalone
  SetOutPath "$INSTDIR"
  File "build\XVBassPedal_artefacts\Release\Standalone\XV Bass Pedal.exe"

  CreateDirectory "$SMPROGRAMS\XV Bass Pedal"
  CreateShortcut "$SMPROGRAMS\XV Bass Pedal\XV Bass Pedal.lnk" "$INSTDIR\XV Bass Pedal.exe"
  CreateShortcut "$SMPROGRAMS\XV Bass Pedal\Desinstalar XV Bass Pedal.lnk" "$INSTDIR\Uninstall.exe"
  CreateShortcut "$DESKTOP\XV Bass Pedal.lnk" "$INSTDIR\XV Bass Pedal.exe"

  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\XVBassPedal" "DisplayName" "XV Bass Pedal v6.0 (VST3 + Standalone)"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\XVBassPedal" "UninstallString" "$\"$INSTDIR\Uninstall.exe$\""
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\XVBassPedal" "Publisher" "XV Audio"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\XVBassPedal" "DisplayVersion" "1.6.0"
SectionEnd

LangString DESC_SecVST3 ${LANG_SPANISH} "Instala XV Bass Pedal.vst3 en C:\Program Files\Common Files\VST3 para Reaper, Ableton Live, FL Studio, Cubase, Studio One y Nuendo."
LangString DESC_SecStandalone ${LANG_SPANISH} "Instala la aplicacion independiente XV Bass Pedal.exe con 11 Presets Custom, Sync Automatico desde GitHub, Afinador, Pitch y Exportador Studio Bi-Amp para M-VAVE Blackbox."

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecVST3} $(DESC_SecVST3)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecStandalone} $(DESC_SecStandalone)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Section "Uninstall"
  RMDir /r "$COMMONFILES64\VST3\XV Bass Pedal.vst3"
  Delete "$INSTDIR\XV Bass Pedal.exe"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  RMDir "$PROGRAMFILES64\XV Audio"

  Delete "$DESKTOP\XV Bass Pedal.lnk"
  RMDir /r "$SMPROGRAMS\XV Bass Pedal"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\XVBassPedal"
SectionEnd
