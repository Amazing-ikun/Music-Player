; SPDX-License-Identifier: Apache-2.0 AND BSD-3-Clause
; ============================================================
; MusicPlayer 安装脚本 (Inno Setup 7)
;
; 安装形态: 用户级安装 —— 不需要管理员(不弹 UAC), 默认装到
;           %LOCALAPPDATA%\Programs\MusicPlayer; 但会显示目录页,
;           允许改到其它盘(如 D:\MusicPlayer)。
;
; 为什么不能装到 C:\Program Files:
;   程序的设置/听歌历史/播放次数都写在自己目录旁的 Data\ 里, 而 Program Files
;   对普通用户不可写, 会导致设置无法保存(每次启动都像第一次用)。
;   脚本已用 NextButtonClick 拦截这种选择。
;
; 打包步骤:
;   cmake --build cmake-build-release
;   cmake --install cmake-build-release --prefix dist
;   "D:\Inno Setup 7\ISCC.exe" installer\MusicPlayer.iss
; ============================================================

#define AppName  "MusicPlayer"
#define AppExe   "MusicPlayer.exe"
#define DistDir  "..\dist"

; 版本号直接读发布 exe 里的 VERSIONINFO(app.rc), 避免与 main.cpp 的
; APP_VERSION 各写一份而互相漂移
#if !FileExists(AddBackslash(SourcePath) + DistDir + "\" + AppExe)
  #error 未找到 dist\MusicPlayer.exe, 请先执行 cmake --install <build> --prefix dist
#endif
#define FullVersion GetVersionNumbersString(AddBackslash(SourcePath) + DistDir + "\" + AppExe)
; RemoveFileExt 砍掉最后一段, 把 "2.1.0.0" 变成 "2.1.0"
#define AppVersion  RemoveFileExt(FullVersion)

[Setup]
AppId={{8E2A5C41-6F3B-4D2E-9A17-5C8B0D4E7F31}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppName}
DefaultDirName={localappdata}\Programs\{#AppName}
DefaultGroupName={#AppName}
; 显示目录页, 让用户能把程序改到别的盘(数据也会跟着落到那里)
DisableDirPage=no
DisableProgramGroupPage=yes
; 用户级安装: 不弹 UAC, 卸载信息写入 HKCU
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\installer-out
OutputBaseFilename={#AppName}_v{#AppVersion}_x64
SetupIconFile=..\icon.ico
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppName} {#AppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
VersionInfoVersion={#FullVersion}
; 许可协议页: 展示 LICENSE(声明 AND 关系与范围, 并指向两份全文); 不接受则无法继续安装
LicenseFile=..\LICENSE
; 程序运行中时提示先关闭 (与 WinMain 里的单实例互斥体同名)
AppMutex=Local\MusicPlayer_SingleInstance
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "chinese"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "附加任务:"; Flags: unchecked

[Files]
; 只装 exe 与 MinGW 运行库; Data\ 状态文件与 .error.log 是运行时产物, 不打包
; BASS 的 DLL 不在本项目中分发(它是 Un4seen 的专有软件), 需用户自行放置, 见 README.md
Source: "{#DistDir}\{#AppExe}";           DestDir: "{app}"; Flags: ignoreversion
Source: "{#DistDir}\libwinpthread-1.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#DistDir}\README.md";           DestDir: "{app}"; Flags: ignoreversion
; 许可证与第三方归属 (Apache-2.0 第 4 条要求随附许可文本)
Source: "{#DistDir}\licenses\*"; DestDir: "{app}\licenses"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}";       Filename: "{app}\{#AppExe}"
Name: "{group}\卸载 {#AppName}";   Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "立即启动 {#AppName}"; Flags: nowait postinstall skipifsilent

[Code]
{ 拦截装到 Program Files 的选择: 那里不可写, 会导致程序无法保存设置 }
function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = wpSelectDir) and
     (Pos('program files', Lowercase(WizardDirValue)) > 0) then
  begin
    MsgBox('不能安装到 Program Files。' + #13#10#13#10 +
           '本程序会把自己的设置与听歌历史写在安装目录下的 Data\ 文件夹里, ' +
           '而 Program Files 对普通用户不可写, 会导致设置无法保存。' + #13#10#13#10 +
           '请改选一个可写目录 (例如 D:\MusicPlayer)。', mbError, MB_OK);
    Result := False;
  end;
end;

{ 安装完成后: 没有 BASS 的 DLL 就无法播放。它不随本安装包提供, 需提示用户自行放置 }
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
    if not FileExists(ExpandConstant('{app}\bass.dll')) then
      MsgBox('MusicPlayer 还需要 BASS 音频库才能播放, 它不随本安装包提供。' + #13#10#13#10 +
             '请从 https://www.un4seen.com/ 下载 BASS、BASS_FX、BASSFLAC, ' +
             '并把 bass.dll、bass_fx.dll、bassflac.dll 复制到:' + #13#10 +
             ExpandConstant('{app}') + #13#10#13#10 +
             '详见该目录下的 README.md。',
             mbInformation, MB_OK);
end;

{ 卸载时询问是否连用户数据一起删除; 默认保留 }
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
  begin
    if MsgBox('是否同时删除设置、听歌历史与错误日志?' + #13#10 +
              '选择"否"会保留它们, 以便日后重新安装时继续使用。',
              mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES then
    begin
      DelTree(ExpandConstant('{app}\Data'), True, True, True);
      DeleteFile(ExpandConstant('{app}\.error.log'));
    end;
  end;
end;
