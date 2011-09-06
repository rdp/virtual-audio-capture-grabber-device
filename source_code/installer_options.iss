#define AppVer "0.2.0"
#define AppName "virtual audio capture grabber device"

[UninstallRun]
Filename: regsvr32; WorkingDir: {app}; Parameters: /s /u audio_sniffer.ax
[Run]
Filename: regsvr32; WorkingDir: {app}; Parameters: /s audio_sniffer.ax
[Files]
Source: Release\audio_sniffer.ax; DestDir: {app}
Source: ..\README.TXT; DestDir: {app}; Flags: isreadme
Source: ..\script_it\*.*; DestDir: {app}\script_it; Flags: recursesubdirs

[Setup]
MinVersion=,6.0.6000
AppName={#AppName}
AppVerName={#AppVer}
DefaultDirName={pf}\virtual audio capture grabber device
DefaultGroupName=virtual audio capture grabber device
UninstallDisplayName=virtual audio capture grabber device uninstall
OutputBaseFilename=setup {#AppName} v{#AppVer}
[Icons]
Name: {group}\Uninstall it; Filename: {uninstallexe}
Name: {group}\Record Using it; Filename: {app}\script_it\record.bat; WorkingDir: {app}\script_it
Name: {group}\Test it by listening to it (causes feedback so close window quickly); Filename: {app}\script_it\feedback.bat; WorkingDir: {app}\script_it
