FFMPEG_SENDER
======================

A StreamSwitch stream sender which is based on the ffmpeg muxing functions.

This sender would get the stream data from a specific StreamSwitch source, 
no matter it's local or remote, and write its media data to a ffmpeg muxing 
context. 

Through the mature ffmpeg library, ffmpeg_sender can support many popular 
stream media protocols, like RTSP(announce method), RTMP(publish), HLS. 


## How to run
----------------------

typing the following command at the this project's root directory (the directory includes this README.md) 
can start up the ffmpeg_sender at front-ground

    # ./ffmpeg_sender -s [stream_name] -f [format_name] -u [URL]

which [stream_name] is the name of the stream published by a StreamSwitch source.
[format_name] is the name of output format to use, if not presented, ffmpeg_sender would guess it from the 
output URL. [URL] is the URL of file to output. 

Besides above, you can get more options by typing following command.

    #./ffmpeg_sender -h
    
You can send SIGINT/SIGTERM signal to the running process to terminate it. 
Also, you can make use of Ctrl+C in the console running ffmpeg_sender to 
terminate it.     
