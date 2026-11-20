#ifndef AppVersion
#define AppVersion "0.1.0"
#endif

[Setup]
AppId={{D31D5F93-18BE-4E38-98C1-6D8F9D9425B3}
AppName=cxxman
AppVersion={#AppVersion}
AppPublisher=tamilarsen-dev
AppPublisherURL=https://github.com/tamilarsen-dev/cxxman
AppUpdatesURL=https://github.com/tamilarsen-dev/cxxman/releases/latest
DefaultDirName={localappdata}\Programs\cxxman
DefaultGroupName=cxxman
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=commandline
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
OutputDir=..\dist
OutputBaseFilename=cxxman-windows-x64-setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
DisableProgramGroupPage=yes
ChangesEnvironment=yes
CloseApplications=yes
RestartApplications=no
UninstallDisplayName=cxxman {#AppVersion}
UninstallDisplayIcon={app}\cxxman.exe

[Files]
Source: "..\dist\cxxman.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\cxxman"; Filename: "{app}\cxxman.exe"; Parameters: "--help"

[Code]
function NormalizePathEntry(const Entry: String): String;
begin
  Result := RemoveBackslashUnlessRoot(Trim(Entry));
end;

function HasPathEntry(const PathValue, Entry: String): Boolean;
var
  Remaining, Part: String;
  Separator: Integer;
begin
  Result := False;
  Remaining := PathValue;
  while Remaining <> '' do
  begin
    Separator := Pos(';', Remaining);
    if Separator = 0 then
    begin
      Part := Remaining;
      Remaining := '';
    end
    else
    begin
      Part := Copy(Remaining, 1, Separator - 1);
      Delete(Remaining, 1, Separator);
    end;
    if CompareText(NormalizePathEntry(Part), NormalizePathEntry(Entry)) = 0 then
      Result := True;
  end;
end;

function RemovePathEntry(const PathValue, Entry: String): String;
var
  Remaining, Part: String;
  Separator: Integer;
begin
  Result := '';
  Remaining := PathValue;
  while Remaining <> '' do
  begin
    Separator := Pos(';', Remaining);
    if Separator = 0 then
    begin
      Part := Remaining;
      Remaining := '';
    end
    else
    begin
      Part := Copy(Remaining, 1, Separator - 1);
      Delete(Remaining, 1, Separator);
    end;
    if CompareText(NormalizePathEntry(Part), NormalizePathEntry(Entry)) <> 0 then
    begin
      if Result <> '' then
        Result := Result + ';';
      Result := Result + Part;
    end;
  end;
end;

procedure UpdateUserPath(const AddEntry: Boolean);
var
  ExistingPath, UpdatedPath, InstallPath: String;
begin
  if not RegQueryStringValue(HKEY_CURRENT_USER, 'Environment', 'Path', ExistingPath) then
    ExistingPath := '';
  InstallPath := ExpandConstant('{app}');
  if AddEntry then
  begin
    if not HasPathEntry(ExistingPath, InstallPath) then
    begin
      if ExistingPath = '' then
        UpdatedPath := InstallPath
      else
        UpdatedPath := ExistingPath + ';' + InstallPath;
      RegWriteExpandStringValue(HKEY_CURRENT_USER, 'Environment', 'Path', UpdatedPath);
    end;
  end
  else
  begin
    UpdatedPath := RemovePathEntry(ExistingPath, InstallPath);
    if UpdatedPath <> ExistingPath then
      RegWriteExpandStringValue(HKEY_CURRENT_USER, 'Environment', 'Path', UpdatedPath);
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
    UpdateUserPath(True);
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
    UpdateUserPath(False);
end;
