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

[Files]
Source: "{#MyAppDistDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Comment: "SnapOCR 截图 / 标注 / 取字"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon
Name: "{userstartup}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Parameters: "--tray"; Tasks: startupicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent
