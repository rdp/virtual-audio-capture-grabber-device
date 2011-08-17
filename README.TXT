This is an audio capture device to capture all the "wave out sound" that is playing on your speakers 
(i.e. record what you hear) for Windows 7/Vista.

To use, download+install it from https://github.com/rdp/virtual-audio-output-sniffer/downloads
then use a sound recording program (example: VLC) choose directshow capture device "Virtual Audio Grabber"

History:

Basically, with windows XP, you can "record what you hear" by selecting stereo mix in your “recording properties” 
and voila, it would record.
With Windows Vista/7, for some reason most sound card drivers do not include this option.
You can check if they do include this option by following the instructions here:
http://downloadsquad.switched.com/2007/01/15/how-to-enable-wave-out-recording-in-vista

If you see "Wave Out Mix" then you're good to go, enable it, and you can use it (you don't need this).
If you don't then you're in luck.

Windows Vista/7 offer a new interface called a "loopback" adapter that captures the outgoing audio just
before it is sent to the speakers [1].

So this little utility captures the loopback audio and offers it to you as an input device
that you can record (via a directshow audio capture device, if that means anything to you).
At least VLC can use this to record, for example.  Ping me if you want me to come up with an easier way to record
or what not, I could come up with an app with "start" and "stop" buttons or what not.

Basically I programmed this as an open source (free) competitor to virtual audio cable.

To install, as an administrator go to the Release subdirectory, and run
C:\...source_code\acam\Release> regsvr32 audio_sniffer.ax

To uninstall:

C:\...source_code\acam\Release> regsvr32 /u audio_sniffer.ax

Troubleshooting.

If you use it in VLC you'll need a directshow cache of at least 40ms, for whatever reason.
Also note that it's tuned currently to work best for recording "what you hear" 
if this doesn't work for you then ping me I can add a "realtime" option or the like.
Basically any feedback welcome, if it doesn't work.
Any feedback welcome, including feature requests like "support more audio channels than 2".

rogerdpack@gmail.com or mailing list http://groups.google.com/group/roger-projects

Some source code from the windows SDK samples, some taken from 
[1] http://blogs.msdn.com/b/matthew_van_eerde/archive/2008/12/16/sample-wasapi-loopback-capture-record-what-you-hear.aspx
