#define AppVer "0.3.2"
#define AppName "Virtual Audio Capture Grabber 32-bit"

[Run]
Filename: {app}\vendor\vcredist_x86.exe; Parameters: "/passive /Q:a /c:""msiexec /qb /i vcredist.msi"" "; StatusMsg: Installing 2010 RunTime...
Filename: regsvr32; WorkingDir: {app}; Parameters: /s audio_sniffer.ax

[UninstallRun]
Filename: regsvr32; WorkingDir: {app}; Parameters: /s /u audio_sniffer.ax

[Files]
Source: source_code\Release\audio_sniffer.ax; DestDir: {app}
Source: README.TXT; DestDir: {app}; Flags: isreadme
Source: screen-capture-recorder-to-video-windows-free\configuration_setup_utility\*.*; DestDir: {app}\screen-capture-recorder-to-video-windows-free\configuration_setup_utility; Flags: recursesubdirs
Source: vendor\vcredist_x86.exe; DestDir: {app}\vendor

[Setup]
MinVersion=,6.0.6000
AppName={#AppName}
AppVerName={#AppVer}
DefaultDirName={pf}\virtual audio capturer
DefaultGroupName=virtual audio capturer
UninstallDisplayName={#AppName} uninstall
OutputBaseFilename=setup {#AppName} v{#AppVer}
OutputDir=releases

[Icons]
Name: {group}\Readme; Filename: {app}\README.TXT
Name: {group}\Uninstall it; Filename: {uninstallexe}
Name: {group}\Record by clicking a start button; Filename: {app}\screen-capture-recorder-to-video-windows-free\configuration_setup_utility\generic_run_rb.bat; WorkingDir: {app}\screen-capture-recorder-to-video-windows-free\configuration_setup_utility; Parameters: record_with_buttons.rb
Name: {group}\Record Using it for x seconds; Filename: {app}\screen-capture-recorder-to-video-windows-free\configuration_setup_utility\generic_run_rb.bat; WorkingDir: {app}\screen-capture-recorder-to-video-windows-free\configuration_setup_utility; Parameters: timed_recording.rb
