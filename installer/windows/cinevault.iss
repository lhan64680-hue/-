#ifndef AppVersion
#define AppVersion "0.1.97"
#endif

#ifndef VersionTag
#define VersionTag "v0.1.97"
#endif

#ifndef SourceDir
#define SourceDir "."
#endif

#ifndef OutputDir
#define OutputDir "."
#endif

[Setup]
AppId={{F3C5C6D6-8B77-4F6A-9F6B-9EA5B6D6AA21}
AppName=影资管家
AppVersion={#AppVersion}
AppVerName=影资管家 {#VersionTag}
AppPublisher=DIT Tools
SetupIconFile=..\..\dit-tools-src\cinevault-pro\resources\icons\app.ico
DefaultDirName={autopf}\影资管家
DefaultGroupName=影资管家
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=CineVault-Setup-{#VersionTag}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
UninstallDisplayIcon={app}\CineVault.exe
SetupLogging=yes
PrivilegesRequired=admin
CloseApplications=yes
RestartApplications=no

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "附加快捷方式："; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Excludes: "data\*"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\data\models\*"; DestDir: "{app}\data\models"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\影资管家"; Filename: "{app}\CineVault.exe"
Name: "{group}\卸载影资管家"; Filename: "{uninstallexe}"
Name: "{autodesktop}\影资管家"; Filename: "{app}\CineVault.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\CineVault.exe"; Description: "启动影资管家"; Flags: nowait postinstall skipifsilent

[Code]
var
  UpdateProgressFilePath: String;
  LastReportedInstallProgress: Integer;

function InitializeSetup(): Boolean;
begin
  UpdateProgressFilePath := ExpandConstant('{param:UPDATEPROGRESSFILE|}');
  LastReportedInstallProgress := -1;
  Result := True;
end;

procedure CurInstallProgressChanged(CurProgress, MaxProgress: Integer);
var
  InstallProgress: Integer;
begin
  if (UpdateProgressFilePath = '') or (MaxProgress <= 0) then
    Exit;

  InstallProgress := Round((CurProgress / MaxProgress) * 100);
  if InstallProgress < 0 then
    InstallProgress := 0
  else if InstallProgress > 100 then
    InstallProgress := 100;

  if InstallProgress <> LastReportedInstallProgress then
  begin
    SaveStringToFile(UpdateProgressFilePath, IntToStr(InstallProgress), False);
    LastReportedInstallProgress := InstallProgress;
  end;
end;
