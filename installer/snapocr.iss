; SnapOCR Inno Setup 安装包脚本（P2 发布项）
;
; 由 scripts\installer.bat 调用：先跑 package.bat 生成 dist\SnapOCR-<版本>\，
; 再用 ISCC 编译本脚本，输出 dist\SnapOCR-<版本>-setup.exe。
;
; 设计取舍：
; - PrivilegesRequired=lowest：默认按当前用户安装到 %LOCALAPPDATA%\Programs，
;   不需要管理员权限（与「不申请管理员权限」的项目约束一致）。
; - 同时提供中文/英文安装向导。
; - 桌面图标与开机自启都是可选任务，默认不勾选。

#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif
#ifndef MyAppVersion4
  #define MyAppVersion4 "0.0.0.0"
#endif

#define MyAppName "SnapOCR"
#define MyAppPublisher "poluce"
#define MyAppExeName "SnapOCR.exe"
#define MyAppDistDir "..\dist\SnapOCR-" + MyAppVersion

[Setup]
AppId={{7F2E4C61-9B3A-4D58-9E21-5A6C7B8D0F13}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
VersionInfoVersion={#MyAppVersion4}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename={#MyAppName}-{#MyAppVersion}-setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
UninstallDisplayIcon={app}\{#MyAppExeName}
SetupLogging=yes

; 中文语言文件来自 Inno Setup 官方仓库（Files/Languages/ChineseSimplified.isl），
; 随本仓库一起分发；缺失时自动退化为仅英文向导。
[Languages]
Name: "chinesesimplified"; MessagesFile: "ChineseSimplified.isl"
#ifexist "Default.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"
#endif

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "startupicon"; Description: "开机自动启动（最小化到托盘）"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "addtopath"; Description: "将安装目录加入 PATH（命令行可直接运行 SnapOCR.exe）"; GroupDescription: "环境与启动"; Flags: checkedonce

[Files]
Source: "{#MyAppDistDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Comment: "SnapOCR 截图 / 标注 / 取字"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon
Name: "{userstartup}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Parameters: "--tray"; Tasks: startupicon

[Registry]
; App Paths：让 Win+R / 资源管理器 / ShellExecute 能按名字找到 SnapOCR.exe（不污染 PATH）
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\App Paths\SnapOCR.exe"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName}"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\App Paths\SnapOCR.exe"; ValueType: string; ValueName: "Path"; ValueData: "{app}"; Flags: uninsdeletekey
; 可选任务：把安装目录追加到当前用户 PATH（不覆盖已有值；卸载时由 [Code] 移除）
Root: HKCU; Subkey: "Environment"; ValueType: expandsz; ValueName: "Path"; ValueData: "{olddata};{app}"; Tasks: addtopath; Check: NeedsAddPath(ExpandConstant('{app}'))

[Run]
; 干净机器缺 MSVC 运行库时自动安装包内的 vc_redist（会触发一次 UAC 授权，
; 因为运行库是系统级组件）。本机已有运行库时这一步自动跳过。
Filename: "{app}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "正在安装 Microsoft Visual C++ 运行库…"; Flags: runhidden waituntilterminated; Check: ShouldInstallVCRedist
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent

[Code]
{ 缺少任意一个 MSVC 运行库 DLL 即认为需要安装（Win10/11 干净系统常见） }
function VCRuntimeMissing(): Boolean;
begin
  Result := False;
  if not FileExists(ExpandConstant('{sys}\vcruntime140.dll')) then Result := True;
  if not FileExists(ExpandConstant('{sys}\vcruntime140_1.dll')) then Result := True;
  if not FileExists(ExpandConstant('{sys}\msvcp140.dll')) then Result := True;
end;

function ShouldInstallVCRedist(): Boolean;
begin
  Result := VCRuntimeMissing()
            and FileExists(ExpandConstant('{app}\vc_redist.x64.exe'));
end;

[Code]
// ---- PATH 处理（可选任务 addtopath）----
// 判定安装目录是否已在当前用户 PATH 中（大小写不敏感，按分号分段比较）。
function NeedsAddPath(Param: string): Boolean;
var
  ExistingPath: string;
begin
  if not RegQueryStringValue(HKEY_CURRENT_USER, 'Environment', 'Path', ExistingPath) then
  begin
    Result := True;
    Exit;
  end;
  Result := Pos(';' + Uppercase(Param) + ';', ';' + Uppercase(ExistingPath) + ';') = 0;
end;

// 卸载时从用户 PATH 中移除安装目录（保留其余路径与 REG_EXPAND_SZ 类型）。
procedure RemoveFromUserPath(Path: string);
var
  ExistingPath: string;
  Remainder: string;
  Segment: string;
  Index: Integer;
begin
  if not RegQueryStringValue(HKEY_CURRENT_USER, 'Environment', 'Path', ExistingPath) then
    Exit;
  Remainder := ExistingPath;
  ExistingPath := '';
  while Remainder <> '' do
  begin
    Index := Pos(';', Remainder);
    if Index = 0 then
    begin
      Segment := Remainder;
      Remainder := '';
    end
    else
    begin
      Segment := Copy(Remainder, 1, Index - 1);
      Remainder := Copy(Remainder, Index + 1, Length(Remainder) - Index);
    end;
    if (Segment <> '') and not SameText(Trim(Segment), Trim(Path)) then
    begin
      if ExistingPath <> '' then
        ExistingPath := ExistingPath + ';';
      ExistingPath := ExistingPath + Segment;
    end;
  end;
  RegWriteExpandStringValue(HKEY_CURRENT_USER, 'Environment', 'Path', ExistingPath);
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
    RemoveFromUserPath(ExpandConstant('{app}'));
end;
