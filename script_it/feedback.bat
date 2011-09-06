echo starting it...
java -jar jruby-complete-1.6.4.jar -e "require 'jruby-swing-helpers/swing_helpers'; SwingHelpers.show_blocking_message_dialog 'We will now playback what is heard, so close this window, then start up some audio program to hear feedback'"

java -jar jruby-complete-1.6.4.jar start_stop_recording_just_desktop.rb ".mp3" audio="virtual-audio-capturer" ffplay