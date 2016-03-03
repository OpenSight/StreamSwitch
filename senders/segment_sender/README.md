SEGMENT_SENDER
======================

A StreamSwitch stream sender outputs streams to a number of separate files 
of nearly fixed duration.

This sender would get the stream data from a specific StreamSwitch source, 
no matter it's local or remote, and save them into a serious segments of the 
specified format, then write these segment to different storage.  

This sender make use of ffmpeg to generate different segment format. and make 
use of different storage driver to support various storage type, 
including POSIX-File, Cloud Storage, FTP and etc. Currently, it support POSIX-File, 
IVR Cloud Storage only. 


## How to run
----------------------

typing the following command at the this project's root directory (the directory includes this README.md) 
can start up the segment_sender at front-ground

    # ./segment_sender -s [stream_name] -f [segment_format] -t [storage_type] -u [URL]

which [stream_name] is the name of the stream published by a StreamSwitch source.
[segment_format] is the name of format to generate segment, default is mpegts. 
[storage_type] is the storage type to save the segments, default is auto, which 
would guess from the [URL]. 
[URL] is the URL of storage to save. [URL]'s format depends on the storage type.

Besides above, you can get more options by typing following command.

    #./segment_sender -h
    
You can send SIGINT/SIGTERM signal to the running process to terminate it. 
Also, you can make use of Ctrl+C in the console running ffmpeg_sender to 
terminate it.     
