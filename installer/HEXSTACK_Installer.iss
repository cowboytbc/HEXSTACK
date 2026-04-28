; ─────────────────────────────────────────────────────────────────────────────
;  HEXSTACK Windows Installer  –  Inno Setup 6
;  Components: VST3 plugin, Standalone app
;  Each component has its own user-selectable destination folder.
; ─────────────────────────────────────────────────────────────────────────────

#define AppName        "HEXSTACK"
#define AppVersion     "1.0"
#define AppPublisher   "INFERNO PLUGINS"
#define AppURL         "https://myinferno.online/"
#define VST3Src        "..\DELIVERABLES\Windows\VST3\HEXSTACK.vst3"
#define StandaloneSrc  "..\DELIVERABLES\Windows\Standalone\HEXSTACK.exe"

; ── Global setup ──────────────────────────────────────────────────────────────
[Setup]
AppId={{B3A1F2C4-5D6E-4A7B-8C9D-0E1F2A3B4C5D}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}

; We manage per-component install dirs via [Code] + custom dir pages,
; so disable the single default dir page.
DefaultDirName={commoncf}\VST3
DefaultGroupName={#AppPublisher}
DisableDirPage=yes

OutputDir=.
OutputBaseFilename=HEXSTACK_Installer_v{#AppVersion}_Windows
SetupIconFile=..\AMP.ico
WizardSmallImageFile=..\AMP.png
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayName={#AppName}
UninstallDisplayIcon={app}\HEXSTACK.exe
MinVersion=10.0

; ── Languages ─────────────────────────────────────────────────────────────────
[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

; ── Components ────────────────────────────────────────────────────────────────
[Components]
Name: "vst3";       Description: "VST3 Plugin  (for DAWs: Ableton, Reaper, FL Studio, etc.)"; Types: full compact custom; Flags: fixed
Name: "standalone"; Description: "Standalone Application  (run without a DAW)";               Types: full custom

; ── Directories ───────────────────────────────────────────────────────────────
; Actual DestDir values are overridden at runtime via {code:GetVST3Dir} / {code:GetStandaloneDir}
[Dirs]
Name: "{code:GetVST3Dir}\HEXSTACK.vst3";  Components: vst3
Name: "{code:GetStandaloneDir}";          Components: standalone

; ── Files ─────────────────────────────────────────────────────────────────────
[Files]
; VST3 bundle
Source: "{#VST3Src}\*"; \
    DestDir: "{code:GetVST3Dir}\HEXSTACK.vst3"; \
    Flags: recursesubdirs createallsubdirs ignoreversion; \
    Components: vst3

; Standalone executable
Source: "{#StandaloneSrc}"; \
    DestDir: "{code:GetStandaloneDir}"; \
    Flags: ignoreversion; \
    Components: standalone

; ── Start Menu shortcuts ──────────────────────────────────────────────────────
[Icons]
Name: "{group}\HEXSTACK (Standalone)"; \
    Filename: "{code:GetStandaloneDir}\HEXSTACK.exe"; \
    Components: standalone
Name: "{group}\Uninstall HEXSTACK"; \
    Filename: "{uninstallexe}"

; ── Desktop shortcut for standalone (optional – user can deselect) ────────────
Name: "{autodesktop}\HEXSTACK"; \
    Filename: "{code:GetStandaloneDir}\HEXSTACK.exe"; \
    Components: standalone; \
    Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut for the Standalone app"; Components: standalone

; ── Uninstall cleanup ─────────────────────────────────────────────────────────
[UninstallDelete]
Type: filesandordirs; Name: "{code:GetVST3Dir}\HEXSTACK.vst3"
Type: files;          Name: "{code:GetStandaloneDir}\HEXSTACK.exe"

; ── Pascal script: custom dir-selection pages ────────────────────────────────
[Code]

var
  VST3DirPage:       TInputDirWizardPage;
  StandaloneDirPage: TInputDirWizardPage;

{ Return the user-chosen VST3 folder (called from [Files]/[Dirs] DestDir) }
function GetVST3Dir(Param: String): String;
begin
  Result := VST3DirPage.Values[0];
end;

{ Return the user-chosen Standalone folder }
function GetStandaloneDir(Param: String): String;
begin
  Result := StandaloneDirPage.Values[0];
end;

procedure InitializeWizard;
begin
  { ── VST3 path page ── }
  VST3DirPage := CreateInputDirPage(
    wpSelectComponents,
    'VST3 Plugin Install Location',
    'Where should the HEXSTACK VST3 plugin be installed?',
    'The folder below is the standard Windows VST3 location. Most DAWs scan here automatically. '
    + 'You can change it if your DAW uses a custom VST3 folder.',
    False, '');
  VST3DirPage.Add('VST3 folder:');
  VST3DirPage.Values[0] := ExpandConstant('{commoncf}\VST3');

  { ── Standalone path page ── }
  StandaloneDirPage := CreateInputDirPage(
    VST3DirPage.ID,
    'Standalone App Install Location',
    'Where should the HEXSTACK Standalone app be installed?',
    'Choose the folder where HEXSTACK.exe will be placed.',
    False, '');
  StandaloneDirPage.Add('Standalone folder:');
  StandaloneDirPage.Values[0] := ExpandConstant('{autopf}\INFERNO TONES\HEXSTACK');
end;

{ Hide the Standalone dir page if the Standalone component is not selected }
function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := False;
  if (PageID = StandaloneDirPage.ID) and (not WizardIsComponentSelected('standalone')) then
    Result := True;
  if (PageID = VST3DirPage.ID) and (not WizardIsComponentSelected('vst3')) then
    Result := True;
end;

{ Confirm before uninstalling }
function InitializeUninstall(): Boolean;
begin
  Result := MsgBox(
    'This will remove HEXSTACK from your computer.'#13#10#13#10
    + 'Your saved .hex preset files will NOT be deleted.'#13#10#13#10
    + 'Continue?',
    mbConfirmation, MB_YESNO) = IDYES;
end;
