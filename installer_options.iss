#define AppVer "0.3.9dev"

#define AppName "Virtual Audio Capture Grabber"

[Run]
; only really need the runtime for the audio_sniffer, the rest uses java/ffmpeg...

Filename: {app}\vendor\vcredist_x86.exe; Parameters: "/passive /Q:a /c:""msiexec /qb /i vcredist.msi"" "; StatusMsg: Installing 2010 RunTime...; MinVersion: 0,6.0.6000
Filename: {app}\vendor\vcredist_x64.exe; Parameters: "/passive /Q:a /c:""msiexec /qb /i vcredist.msi"" "; StatusMsg: Installing 2010 64 bit RunTime...; MinVersion: 0,6.0.6000; Check: IsWin64
Filename: regsvr32; WorkingDir: {app}; Parameters: /s audio_sniffer.dll; MinVersion: 0,6.0.6000
Filename: regsvr32; WorkingDir: {app}; Parameters: /s audio_sniffer-x64.dll; MinVersion: 0,6.0.6000; Check: IsWin64

[UninstallRun]
Filename: regsvr32; WorkingDir: {app}; Parameters: /s /u audio_sniffer.dll; MinVersion: 0,6.0.6000
Filename: regsvr32; WorkingDir: {app}; Parameters: /s /u audio_sniffer-x64.dll; MinVersion: 0,6.0.6000; Check: IsWin64

[Files]
Source: source_code\Release\audio_sniffer.dll; DestDir: {app}
Source: source_code\x64\Release\audio_sniffer-x64.dll; DestDir: {app}
Source: README.TXT; DestDir: {app}; Flags: isreadme
Source: screen-capture-recorder-to-video-windows-free\configuration_setup_utility\*.*; DestDir: {app}\screen-capture-recorder-to-video-windows-free\configuration_setup_utility; Flags: recursesubdirs
Source: screen-capture-recorder-to-video-windows-free\vendor\vcredist_x86.exe; DestDir: {app}\vendor
Source: screen-capture-recorder-to-video-windows-free\vendor\vcredist_x64.exe; DestDir: {app}\vendor

[Setup]
; disabled ; MinVersion=,6.0.6000
AppName={#AppName}
AppVerName={#AppVer}
DefaultDirName={pf}\{#AppName}
DefaultGroupName={#AppName}
UninstallDisplayName={#AppName} uninstall
OutputBaseFilename=setup {#AppName} v{#AppVer}
OutputDir=releases

[Icons]
Name: {group}\Readme; Filename: {app}\README.TXT
Name: {group}\Uninstall {#AppName}; Filename: {uninstallexe}
Name: {group}\Record\Record by clicking a start button; Filename: {app}\screen-capture-recorder-to-video-windows-free\configuration_setup_utility\generic_run_rb.bat; WorkingDir: {app}\screen-capture-recorder-to-video-windows-free\configuration_setup_utility; Parameters: record_with_buttons.rb --just-audio-default; Flags: runminimized
Name: {group}\Broadcast\setup local audio streaming server; Filename: {app}\screen-capture-recorder-to-video-windows-free\configuration_setup_utility\generic_run_rb.bat; WorkingDir: {app}\screen-capture-recorder-to-video-windows-free\configuration_setup_utility; Parameters: broadcast_server_setup.rb; Flags: runminimized
Name: {group}\Broadcast\restart local audio streaming server with same setup as was run previous; Filename: {app}\screen-capture-recorder-to-video-windows-free\configuration_setup_utility\generic_run_rb.bat; WorkingDir: {app}\screen-capture-recorder-to-video-windows-free\configuration_setup_utility; Parameters: broadcast_server_setup.rb --redo-with-last-run; Flags: runminimized
