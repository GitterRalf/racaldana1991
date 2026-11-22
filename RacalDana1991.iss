; Build RacalDana1991.exe and deploy its Qt runtime with build.ps1 before compiling.
; The NI-488.2 driver is installed separately.
#define ReadQuoted(Text) Copy(Text, Pos('"', Text) + 1, Pos('"', Copy(Text, Pos('"', Text) + 1, Len(Text))) - 1)
#define ProjectFile FileOpen(SourcePath + "CMakeLists.txt")
#expr FileRead(ProjectFile)
#define ProjectDeclaration FileRead(ProjectFile)
#define VersionTail Copy(ProjectDeclaration, Pos(" VERSION ", ProjectDeclaration) + Len(" VERSION "), Len(ProjectDeclaration))
#define RacalDana1991Version Copy(VersionTail, 1, Pos(" ", VersionTail) - 1)
#expr FileClose(ProjectFile)
#define IconResource FileOpen(SourcePath + "assets\app.rc")
#define RacalDana1991Icon ReadQuoted(FileRead(IconResource))
#expr FileClose(IconResource)
#ifndef BuildArch
  #define BuildArch "x64"
#endif
#if BuildArch == "x86"
  #define RacalDana1991Name "Racal-Dana 1991 Control (32 Bit)"
  #define DeviceAppId "{{7AB5875A-6F7D-54BA-92C2-A9BB7D6E7851}"
  #define DeviceBuildDir "build-x86"
#elif BuildArch == "x64"
  #define RacalDana1991Name "Racal-Dana 1991 Control"
  #define DeviceAppId "{{EE0EB66F-1FF9-4E15-9C1F-E6C5652F2A56}"
  #define DeviceBuildDir "build"
#else
  #error BuildArch must be x86 or x64.
#endif
#define RacalDana1991Exe "RacalDana1991.exe"

[Setup]
AppId={#DeviceAppId}
AppName={#RacalDana1991Name}
AppVersion={#RacalDana1991Version}
AppVerName={#RacalDana1991Name} {#RacalDana1991Version}
AppPublisher=Ralf Tralow
VersionInfoVersion={#RacalDana1991Version}
VersionInfoDescription={#RacalDana1991Name} Setup
DefaultDirName={pf}\{#RacalDana1991Name}
DefaultGroupName={#RacalDana1991Name}
AllowNoIcons=yes
PrivilegesRequired=admin
#if BuildArch == "x64"
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
#else
ArchitecturesAllowed=x86 x64
#endif
; Inno 5.3.5 can see modern Windows as 6.2 due to its legacy manifest.
SetupIconFile=assets\{#RacalDana1991Icon}
UninstallDisplayIcon={app}\{#RacalDana1991Exe}
OutputDir=installer
OutputBaseFilename=RacalDana1991-Setup-{#RacalDana1991Version}-{#BuildArch}
Compression=lzma
SolidCompression=yes

[Languages]
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; Flags: unchecked

[Files]
#if BuildArch == "x86"
Source: "{#DeviceBuildDir}\qt.conf"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#DeviceBuildDir}\bearer\*.dll"; DestDir: "{app}\bearer"; Flags: ignoreversion skipifsourcedoesntexist
#endif
Source: "{#DeviceBuildDir}\{#RacalDana1991Exe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#DeviceBuildDir}\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#DeviceBuildDir}\generic\*.dll"; DestDir: "{app}\generic"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#DeviceBuildDir}\iconengines\*.dll"; DestDir: "{app}\iconengines"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#DeviceBuildDir}\imageformats\*.dll"; DestDir: "{app}\imageformats"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#DeviceBuildDir}\networkinformation\*.dll"; DestDir: "{app}\networkinformation"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#DeviceBuildDir}\platforms\*.dll"; DestDir: "{app}\platforms"; Flags: ignoreversion
Source: "{#DeviceBuildDir}\styles\*.dll"; DestDir: "{app}\styles"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#DeviceBuildDir}\tls\*.dll"; DestDir: "{app}\tls"; Flags: ignoreversion skipifsourcedoesntexist
Source: "LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "THIRD-PARTY-NOTICES.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "licenses\*.txt"; DestDir: "{app}\licenses"; Flags: ignoreversion

[Icons]
Name: "{group}\{#RacalDana1991Name}"; Filename: "{app}\{#RacalDana1991Exe}"; WorkingDir: "{app}"
Name: "{commondesktop}\{#RacalDana1991Name}"; Filename: "{app}\{#RacalDana1991Exe}"; WorkingDir: "{app}"; Tasks: desktopicon
