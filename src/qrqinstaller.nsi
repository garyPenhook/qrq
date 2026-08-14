!define src "."
!define startmenu "$SMPROGRAMS\qrq"
!define appkey "Software\qrq"
!define uninstallkey "Software\Microsoft\Windows\CurrentVersion\Uninstall\qrq"

!include "x64.nsh"

Name "qrq"
Caption "qrq _VERSION_ - High Speed CW Trainer"
OutFile "qrq-_VERSION_.exe"
Icon "qrq.ico"
RequestExecutionLevel admin
 
InstallDir "$PROGRAMFILES64\qrq"
InstallDirRegKey HKLM "${appkey}" "InstallLocation"
DirText "Setup will install qrq _VERSION_ in the following folder. To install in a different folder, click Browse and select another folder. Click Install to start the installation. Note that if qrq is already installed the old installation is overwritten, except for the files 'qrqrc' and 'toplist'."
 
LicenseText "License of High Speed CW trainer 'qrq' (https://fkurz.net/ham/qrq.html) by Fabian Kurz, DJ5CW"
LicenseData "COPYING"
 
Page license
Page directory
Page instfiles
 
UninstPage uninstConfirm
UninstPage instfiles
 
AutoCloseWindow false
ShowInstDetails show

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "This installer requires 64-bit Windows."
    Abort
  ${EndIf}
FunctionEnd

Section
	SetShellVarContext all
	SetOutPath $INSTDIR
	File "qrq.exe"
	File /oname=COPYING.txt "COPYING"
	File /oname=AUTHORS.txt "AUTHORS"
	File "callbase.qcb"
	File /oname=ChangeLog.txt "ChangeLog"
	File "english.qcb"
	File "cwops.qcb"
	File "morserunner.qcb"
	File /oname=README.txt "README"
  	SetOverwrite off
	File "qrqrc"
	File "toplist"
  	SetOverwrite on
	WriteUninstaller "qrquninstall.exe"
	WriteRegStr HKLM "${appkey}" "InstallLocation" "$INSTDIR"
	WriteRegStr HKLM "${uninstallkey}" "DisplayName" "qrq"
	WriteRegStr HKLM "${uninstallkey}" "DisplayVersion" "_VERSION_"
	WriteRegStr HKLM "${uninstallkey}" "DisplayIcon" "$INSTDIR\qrq.exe"
	WriteRegStr HKLM "${uninstallkey}" "InstallLocation" "$INSTDIR"
	WriteRegStr HKLM "${uninstallkey}" "Publisher" "Fabian Kurz, DJ5CW; W4GNS fork"
	WriteRegStr HKLM "${uninstallkey}" "URLInfoAbout" "https://github.com/garyPenhook/qrq"
	WriteRegStr HKLM "${uninstallkey}" "UninstallString" "$\"$INSTDIR\qrquninstall.exe$\""
	WriteRegDWORD HKLM "${uninstallkey}" "NoModify" 1
	WriteRegDWORD HKLM "${uninstallkey}" "NoRepair" 1
SectionEnd
 
Section
  CreateDirectory "${startmenu}"
  SetOutPath $INSTDIR
  CreateShortCut "${startmenu}\qrq.lnk" "$INSTDIR\qrq.exe"
  CreateShortCut "$DESKTOP\qrq.lnk" "$INSTDIR\qrq.exe"
  CreateShortCut "${startmenu}\uninstall_qrq.lnk" "$INSTDIR\qrquninstall.exe"
  WriteINIStr "${startmenu}\qrq.url" "InternetShortcut" "URL" "https://fkurz.net/ham/qrq.html"
SectionEnd
 
; Uninstaller
; All section names prefixed by "Un" will be in the uninstaller
 
UninstallText "This will completely uninstall qrq."
 
Section "Uninstall"
  SetShellVarContext all
  Delete "${startmenu}\*.*"
  RMDir "${startmenu}"
  Delete "$DESKTOP\qrq.lnk"
  Delete "$INSTDIR\*.*"
  RMDir $INSTDIR
  DeleteRegKey HKLM "${uninstallkey}"
  DeleteRegKey HKLM "${appkey}"
SectionEnd

