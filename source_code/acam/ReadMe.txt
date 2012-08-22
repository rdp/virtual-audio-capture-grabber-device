TODO:
msvad is the only way for WME probably...

// TODO tell them here: http://social.msdn.microsoft.com/forums/en-us/windowsdirectshowdevelopment/thread/F9F656CC-FBD5-4F78-B2D5-EF3214274795
// http://msdn.microsoft.com/en-us/library/dd317587%28VS.85%29.aspx lists some more filter interfaces...

http://blogs.msdn.com/b/matthew_van_eerde/archive/2011/09/09/how-to-validate-and-log-a-waveformatex.aspx maybe to output what other things give us?

maybe a straight verbatim copy instead of my 16-bit hack? Except did things like VLC need that to work?
WAVE_FORMAT_EXTENSIBLE

maybe try these, see if they work:
http://betterlogic.com/roger/2010/07/directsound-audio-input-filter-exampledemo/




big buffer: blips 2, 3
a little "fast forward snip" at the beginning when it's catching up (and has tons of blips)...

with threads 1, full size:
  a few fast forwards...


with threads 2:
lots of fast forward blips at the beginning, blips through, "stumbles forward"

with threads 2, actually using it, still just 2 blips
I think it's ok to use.