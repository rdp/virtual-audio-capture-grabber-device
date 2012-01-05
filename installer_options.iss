#define AppVer "0.2.6"
#define AppName "virtual audio capture grabber device"

[UninstallRun]
Filename: regsvr32; WorkingDir: {app}; Parameters: /s /u audio_sniffer.ax
[Run]
Filename: regsvr32; WorkingDir: {app}; Parameters: /s audio_sniffer.ax

[Files]
Source: source_code\Release\audio_sniffer.ax; DestDir: {app}
Source: README.TXT; DestDir: {app}; Flags: isreadme
Source: script_it\*.*; DestDir: {app}\script_it; Flags: recursesubdirs

[Setup]
MinVersion=,6.0.6000
AppName={#AppName}
AppVerName={#AppVer}
DefaultDirName={pf}\virtual audio capturer
DefaultGroupName=virtual audio capturer
UninstallDisplayName={#AppName} uninstall
OutputBaseFilename=setup {#AppName} v{#AppVer}

[Icons]
Name: {group}\Readme; Filename: {app}\README.TXT
Name: {group}\Uninstall it; Filename: {uninstallexe}
Name: {group}\Record Using it for x seconds; Filename: {app}\script_it\record.bat; WorkingDir: {app}\script_it
Name: {group}\Test it by listening to it (DANGER causes feedback); Filename: {app}\script_it\feedback.bat; WorkingDir: {app}\script_it
