#define MyAppName "Voltix"
#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\dist"
#endif
#ifndef OutputDir
  #define OutputDir "..\package"
#endif

[Setup]
; Cet identifiant ne doit JAMAIS changer : c'est lui qui permet les mises a jour par-dessus.
AppId={{8F2B6C1E-4D3A-4E5B-9A7C-1B2D3E4F5A6B}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher=Voltix
DefaultDirName={autopf}\Voltix
DefaultGroupName=Voltix
UninstallDisplayIcon={app}\bin\Voltix.exe
OutputDir={#OutputDir}
OutputBaseFilename=Voltix-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequiredOverridesAllowed=dialog
ChangesAssociations=yes

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Tasks]
Name: "desktopicon"; Description: "Creer un raccourci sur le Bureau"; Flags: unchecked
Name: "fileassoc"; Description: "Associer les fichiers .voltix a Voltix"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{group}\Voltix"; Filename: "{app}\bin\Voltix.exe"
Name: "{group}\Desinstaller Voltix"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Voltix"; Filename: "{app}\bin\Voltix.exe"; Tasks: desktopicon

[Registry]
Root: HKA; Subkey: "Software\Classes\.voltix\OpenWithProgids"; ValueType: string; ValueName: "Voltix.Schema"; ValueData: ""; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKA; Subkey: "Software\Classes\Voltix.Schema"; ValueType: string; ValueName: ""; ValueData: "Schema Voltix"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKA; Subkey: "Software\Classes\Voltix.Schema\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\bin\Voltix.exe"" ""%1"""; Tasks: fileassoc

[Run]
Filename: "{app}\bin\Voltix.exe"; Description: "Lancer Voltix"; Flags: nowait postinstall skipifsilent
; mise a jour automatique (installation silencieuse lancee par Voltix) : on relance Voltix a la fin
Filename: "{app}\bin\Voltix.exe"; Flags: nowait skipifnotsilent runasoriginaluser