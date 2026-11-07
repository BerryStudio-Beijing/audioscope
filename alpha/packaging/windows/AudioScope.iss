; AudioScope by BerryStudio 0.9.1-beta — Inno Setup 6 installer.
; Build from a directory that contains AudioScope.exe, the docs and this file:
;   ISCC.exe AudioScope.iss
; The release workflow does exactly that after the MSVC build.

#define AppName "AudioScope by BerryStudio"
#define AppVersion "0.9.1-beta.1"
#define AppPublisher "BerryStudio"
#define AppExe "AudioScope.exe"

[Setup]
AppId={{A7F1C0D2-5B7E-4C3A-9F21-8D6E4B2A1C90}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\AudioScope
DefaultGroupName=AudioScope
DisableProgramGroupPage=yes
OutputDir=.
OutputBaseFilename=AudioScope-0.9.1-beta.1-windows-x64-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
UninstallDisplayIcon={app}\{#AppExe}
PrivilegesRequired=admin

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional tasks:"; Flags: unchecked

[Files]
Source: "{#AppExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "KNOWN-ISSUES.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "CHANGELOG.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "VERSION"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "Launch {#AppName}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}"
