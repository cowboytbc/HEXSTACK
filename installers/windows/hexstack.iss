; HEXSTACK Plugin Installer for Windows
; Inferno Plugins
; Built with Inno Setup 6

#ifndef MyAppVersion
  #define MyAppVersion "1.0.0"
#endif

#define MyAppName      "HEXSTACK"
#define MyAppPublisher "Inferno Plugins"
#define MyAppURL       "https://github.com/cowboytbc/HEXSTACK"

[Setup]
AppId={{4F9B2E1A-C837-4D52-B891-A3F07E6D5C22}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
; Standalone app installs here (user can change on install wizard)
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
; Output the installer .exe to ../../release/ relative to this script
OutputDir=..\..\release
OutputBaseFilename=HEXSTACK-Windows-{#MyAppVersion}-Setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
; Require admin so VST3 can be written to Program Files\Common Files\VST3
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible
DisableDirPage=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full";       Description: "Full Installation  (VST3 Plugin + Standalone App)"
Name: "vst3only";   Description: "VST3 Plugin Only"
Name: "standalone"; Description: "Standalone Application Only"
Name: "custom";     Description: "Custom Installation"; Flags: iscustom

[Components]
Name: "vst3";       Description: "VST3 Plugin{break}Installs HEXSTACK.vst3 to Common Files\VST3 for use in any DAW"; Types: full vst3only
Name: "standalone"; Description: "Standalone Application{break}Run HEXSTACK without a DAW"; Types: full standalone

[Files]
; VST3 bundle — Source paths are relative to this .iss file (installers\windows\)
; so ..\..\installer-staging\ resolves to the project root installer-staging\
Source: "..\..\installer-staging\VST3\HEXSTACK.vst3\*"; \
  DestDir: "{commoncf64}\VST3\HEXSTACK.vst3"; \
  Flags: ignoreversion recursesubdirs createallsubdirs; \
  Components: vst3

; Standalone executable
Source: "..\..\installer-staging\Standalone\HEXSTACK.exe"; \
  DestDir: "{app}"; \
  Flags: ignoreversion; \
  Components: standalone

[Icons]
Name: "{group}\HEXSTACK";          Filename: "{app}\HEXSTACK.exe";  Components: standalone
Name: "{group}\Uninstall HEXSTACK"; Filename: "{uninstallexe}"
Name: "{commondesktop}\HEXSTACK";  Filename: "{app}\HEXSTACK.exe";  Components: standalone; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; \
  Description: "Create a &desktop shortcut for HEXSTACK"; \
  GroupDescription: "Additional icons:"; \
  Components: standalone; \
  Flags: unchecked

[Run]
Filename: "{app}\HEXSTACK.exe"; \
  Description: "Launch HEXSTACK now"; \
  Flags: nowait postinstall skipifsilent; \
  Components: standalone

[UninstallDelete]
; Remove the VST3 folder on uninstall (Inno Setup only removes files it installed)
Type: filesandordirs; Name: "{commoncf64}\VST3\HEXSTACK.vst3"
