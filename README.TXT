This is an audio capture device allowing you to capture all the "wave out sound" that is playing on your speakers 
(i.e. record what you hear) for Windows 7/Vista.  Windows XP users please read the "history" section
for something else you can try in its place.

NB THAT IT IS USED WITHIN/INCLUDED IN THE MORE FULL FEATURED https://github.com/rdp/screen-capture-recorder-to-video-windows-free

== Installation ==

To use, download+install here: https://github.com/rdp/virtual-audio-capture-grabber-device/downloads
then use it via its included "record for x seconds" utility, or
via any other program that can take directshow devices as an input.

  If you want other options for its use, like broadcasting, or saving from it or any specific audio device input (ex: waveout with Windows XP)
  then download this utility: https://github.com/rdp/screen-capture-recorder-to-video-windows-free
  which has has more options and a different/better UI for broadcast/recording.

 NB that you may need need to install the MSVC 2010 runtime distro first, if it's not already
  installed on your machine (typically, it already is).
  http://www.microsoft.com/download/en/details.aspx?id=5555

== Usage ==

Example: VLC media player: media [menu] -> open capture device -> select capture audio device "virtual-audio-capturer"

Example: ffmpeg (to save audio to file yo.mp3 from what is currently playing, requires 32 bit ffmpeg, 64 bit DLL available upon demand):
$ ffmpeg  -f dshow -i audio="virtual-audio-capturer" yo.mp3

(also see https://github.com/rdp/screen-capture-recorder-program if you want to stream your desktop with audio
as well or capture it or the like)

NB that you'll need java JRE previously installed to use the included helper apps.

== Troubleshooting ==

Also note that VLC directshow input might need buffers of at least 40 or 50 ms or it fails for some reason.

Notes from users:

"I was able to reduce the VLC delay messages by changing the output frame rate from 30 to 24 and increasing the dsow buffer from 200 to 20000"

If I make the "PlayOn Virtual Audio Device" or the "Virtual Audio Cable" the Playback Default device instead of the Realtek speakers all the problems with your filter go away. So it would seem the problem is really with the Realtek Driver. 

= History/XP users =

Basically, with windows XP, you can typically already "record what you hear"
Run sndvol32 (Start menu -> Accessories -> Volume/Audio), choose options [menu] -> properties -> recording radio button, click ok, 
now click the "select" checkbox underneath "Wave Out Mix"

Now with for example VLC choose "Open Capture Device" -> Audio Device Name -> select "your soundcard's name" (other
programs just select your soundcard's name).

With Windows Vista/7, for some reason many sound card drivers do not include this as an option.
Some do though. You can check if yours already does by following instructions here:
http://downloadsquad.switched.com/2007/01/15/how-to-enable-wave-out-recording-in-vista
You may want to download and install new audio drivers first before looking for it, sometimes that helps.

If you're in Vista/Windows 7 and see a "Wave Out Mix" following the above instructions
then you're good to go: enable it, and you can use it (you thus don't need virtual-audio-capture-grabber-device at
all).

If you don't see it then you're in luck this, device is for you.
Windows Vista/7 offer a new interface called a "loopback" adapter that captures the outgoing audio just
as it is sent to your speakers [1].

This little utility captures the loopback audio and offers it to you as an input device
that you can then record (it's captured as a directshow audio capture device, if that means anything to you).
VLC can use this to record, for example.  There are some example apps included with this package.

Basically I programmed this as an open source (free) competitor to virtual audio cable.
Ping me if you want me to convert it into a "real" kernel level audio device so that any program can use it,
not just directshow compatible ones.
Ping me if you want an easier "start/stop" recorder, too.


Another option is "If your sound card doesn't have the option to record what you hear, 
use a cable (with 3.5mm headphone jacks on both ends) to connect the line out of your 
sound card to the line in (using a splitter if you need to be able to hear what you're doing, 
and disabling mic boost if you use a microphone input)."

But that's hardware and this is a software answer :)



==Troubleshooting/feedback ==

If you use it in VLC you'll need a directshow cache of at least 40ms, for whatever reason.
Also note that it's tuned set to work best for recording "what you hear" 
if this doesn't work for you then ping me I maybe could add a more "realtime" option or 
something.
Basically any feedback welcome, if it doesn't work.
Also note that if you turn down your system volume within windows, it will still continue recording
or playing, as apparently it captures it at "normal volume" regardless of how high your speaker output is.
Any feedback welcome, including feature requests like "support more audio channels than 2".

If you want a smaller download you can just download the file source_code\Release\audio_sniffer.ax then run regsvr32 audio_sniffer.ax as an admin user.

rogerdpack@gmail.com or mailing list http://groups.google.com/group/roger-projects

== attribution ==

Some source code originally from the windows SDK samples, some taken from [1]
So you'll probably need to install the Windows SDK before playing around with the source code, legally.

Also you may find this project, a directshow screen capture device, similarly useful:
https://github.com/rdp/on-screen-capture-recorder-to-video-windows-free

Enjoy!

[1] http://blogs.msdn.com/b/matthew_van_eerde/archive/2008/12/16/sample-wasapi-loopback-capture-record-what-you-hear.aspx