; ============================================================================
;  Macro Manager - Inno Setup script
;  Builds MacroManagerSetup.exe from the Release x64 build output.
;
;  How to use:
;    1. Build the app in Visual Studio: Release / x64
;       (output lands in ..\x64\Release\ next to this installer folder)
;    2. Install Inno Setup: https://jrsoftware.org/isdl.php
;    3. Open this file in the Inno Setup Compiler and click Build (F9),
;       or from a terminal:  iscc installer\MacroManager.iss
;    4. The finished installer appears in installer\Output\MacroManagerSetup.exe
; ============================================================================

#define MyAppName        "Macro Manager"
#define MyAppVersion     "1.0.2"
#define MyAppPublisher   "Ralzify"
#define MyAppURL         "https://github.com/Ralzify/MacroManager"
#define MyAppExeName     "Macro Manager.exe"

; Folder holding the built files (exe + chimes). Adjust if your build output differs.
#define BuildDir         "..\x64\Release"
; Icon source (lives in the Macro source folder).
#define IconFile         "..\Macro\app.ico"

[Setup]
; A unique ID for this app. Keep this constant across versions so upgrades
; replace the previous install instead of creating a duplicate.
AppId={{A7F3C2E1-9B4D-4E6A-8C12-3F5D6E7A8B90}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}/releases

; Install into Program Files (requires admin elevation).
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppName}

; Only allow installing on 64-bit Windows (the build is x64).
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

; Wizard / output settings.
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupIconFile={#IconFile}
OutputDir=Output
OutputBaseFilename=MacroManagerSetup
PrivilegesRequired=admin
DisableProgramGroupPage=yes
; LicenseFile=..\LICENSE        ; uncomment once you add a LICENSE file

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#BuildDir}\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\enabled.mp3";     DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\disabled.mp3";    DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}";        Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}";  Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
; Offer to launch the app when the installer finishes.
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
