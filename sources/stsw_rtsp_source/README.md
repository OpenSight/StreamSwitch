STSW_RTSP_SOURCE
======================

A live StreamSwitch source program with standard RTSP protocol (RFC 2326) support. 

This source make use of a RTSP client to get the Media data from a RTSP server and broadcast them to all subscriber StreamSwitch Sinks

Currently, this source is based on a modified LIVE555 media library (original version date: 2015.3.19) whose source code is in the thirdparty directory. 
LIVE555 media library should be installed under /usr/local/lib and the header files should be install under /usr/local/include/liveMedia. swst_rtsp_source 
make use of these path to include the header and lib.


## How to run
----------------------

typing the following command at the STSW_RTSP_SOURCE project's root directory (the directory includes this README.md) 
can start up the stsw_rtsp_source daemon at front-ground

    # ./stsw_rtsp_source -s [stream_name] -u [RTSP URL]

which [stream_name] is the name of the stream published by this source instance, 
[RTSP URL] is the RTSP URL to access the remote stream. Besides above, you can get more
options by typing following command.

    #./stsw_rtsp_source -h
    
You can send SIGINT/SIGTERM signal to the running process to terminate it. 
Also, you can make use of Ctrl+C in the console running stsw_rtsp_source to 
terminate it.     

    


