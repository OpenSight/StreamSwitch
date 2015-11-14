FFMPEG_DEMUXER_SOURCE
======================

A special StreamSwitch source which is based on the ffmpeg demuxing function. 

This source get the stream data from a ffmpeg demuxer context, 
and publish it as a StreamSwitch Source. By making use of ffmpeg demuxing, 
this source can support various source format or standard live stream 
protocols, like RTSP(PLAY mode), RTMP(client or server), TCP/UDP。 


## How to run
----------------------

typing the following command at the FFMPEG_DEMUXER_SOURCE project's root directory (the directory includes this README.md) 
can start up the stsw_proxy_source at front-ground

    # ./ffmpeg_demuxer_source -s [stream_name] -u [URL]

which [stream_name] is the name of the stream published by this source instance, 
[URL] is the URL of the input file for ffmpeg demuxing context, you can get more
options by typing following command.

    #./ffmpeg_demuxer_source -h
    
You can send SIGINT/SIGTERM signal to the running process to terminate it. 
Also, you can make use of Ctrl+C in the console running stsw_proxy_source to 
terminate it.     
