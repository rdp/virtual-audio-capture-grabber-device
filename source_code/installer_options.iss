#define AppVer "0.1"
#define AppName "virtual audio capture grabber device"

[UninstallRun]
Filename: regsvr32; WorkingDir: {app}; Parameters: /u audio_sniffer.ax
[Run]
Filename: regsvr32; WorkingDir: {app}; Parameters: audio_sniffer.ax
[Files]
Source: C:\dev\ruby\virtual-audio-output-sniffer\source_code\Release\audio_sniffer.ax; DestDir: {app}
Source: ..\README.TXT; DestDir: {app}; Flags: isreadme
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
