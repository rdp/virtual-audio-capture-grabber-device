= synchro audio =

TODO test, watch it get more and more off

ask MSDN "umm....should the default reference clock be following wall time? Or better, if I'm doing live audio capture, should I adjust the 
default reference clock using to match my incoming samples using SetTimeDelta?"

http://blogs.msdn.com/b/medmedia/archive/2007/10/02/more-about-a-v-synchronization-in-dshow.aspx

"wait" till I should give them out?
  advisetime is a wait.
  this can't work, right?

"Had a similar problem [things get more and more behind].  Tim Buryi at Blackmagic suggested adding an audio capture filter with default renderer and using the audio clock as the reference clock for the graph.  Seems to work well."

set source null (no graph?)

http://social.msdn.microsoft.com/forums/en-us/windowsdirectshowdevelopment/thread/659371F9-446A-4A17-8264-3CDFBC30279A "what do you know, it was the reference clock killing me" or something...

not stamp?





TODO:
msvad

The DirectSound audio renderer also alters it's playback rate by a small percentage in an effort to stay synced with the graph clock, reducing the chance of a buffer overrun or underrun.


notes:
big buffer: blips 2, 3
a little "fast forward snip" at the beginning when it's catching up (and has tons of blips)...

with threads 1, full size:
  a few fast forwards...


with threads 2:
lots of fast forward blips at the beginning, blips through, "stumbles forward"

with threads 2, actually using it, still just 2 blips
I think it's ok to use.