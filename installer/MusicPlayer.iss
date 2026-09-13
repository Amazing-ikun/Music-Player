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
; 只装 exe 与运行时 DLL; Data\ 状态文件与 .error.log 是运行时产物, 不打包
Source: "{#DistDir}\{#AppExe}";           DestDir: "{app}"; Flags: ignoreversion
Source: "{#DistDir}\bass.dll";            DestDir: "{app}"; Flags: ignoreversion
Source: "{#DistDir}\bass_fx.dll";         DestDir: "{app}"; Flags: ignoreversion
Source: "{#DistDir}\bassflac.dll";        DestDir: "{app}"; Flags: ignoreversion
; MinGW 运行库(静态 libstdc++ 仍引用 winpthread)
Source: "{#DistDir}\libwinpthread-1.dll"; DestDir: "{app}"; Flags: ignoreversion
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
