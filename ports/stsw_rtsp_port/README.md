STSW_RTSP_PORT
======================

A StreamSwitch Port with standard RTSP protocol (RFC 2326) support. 

This port program works as a RTSP Server, service each RTSP client connection 
with a forked child process. It can transmits the live stream by RTP/RTSP as 
well as the replay stream, depending RTSP URL in the request. 

This port is derived from the "feng" (2.1.0), which is a open source 
lightweight RTSP server,  
and then is supplemented with our own extension to make it compatible with 
StreamSwitch and work as a StreamSwitch port.

## Depends
-----------------------

As a "feng" derivative, it depends the following library (open source): 

1. libev >= 4.0
2. libnetembryo >= 0.1.0
3. glib-2.0 >= 2.16

## CODEC
-----------------------

By now, stsw_rtsp_port support the following codec for different stream type. 

1. video
    * H264 (supported by RAW, MP2P)
    * MP4V-ES (supported by RAW, MP2P)

2. audio
    * PCMU (supported by RAW, MP2P)
    * PCMA (supported by RAW, MP2P)
    * AAC (supported by RAW)


## RTSP URL
-----------------------

RTSP Client can indicate which stream to receive by the RTSP URL in its request. 
The RTSP URL used for stsw_rtsp_port server follows the below pattern.

    rtsp://[server_ip]:[port]/stsw/stream/[stream_name]?param1=value&param2=value...
      
Where [server_ip] is the IP or domain name of the server running stsw_rtsp_port, 
[port] is the RTSP listening port of stsw_rtsp_port, default is 554 for rtsp, 
[stream_name] is the name of the specific stream to receive by RTP/RTSP. 
Some parameters can be specified in the RTSP URL. By now, stsw_rtsp_port support the 
following parameter.

host - if the stream source is running on the other host than the one running 
       stsw_rtsp_port, this parameter specify the IP of the remote host. 
       If this parameter absent, the stream source is considered running on
       the same host with stsw_rtsp_port. 
port - the port of the stream source which running on the other host, only valid if 
       host is given.
stream_type - which type of the mux stream emitted by this RTSP session, can 
              only be RAW/MP2P by now. Default is configured from the command 
              arguments of stsw_rtsp_port.   
              

## How to run
--------------------------

Typing the following command at this directory can start the stsw_rtsp_port at 
front-ground with outputting log to stderr, and never exit: 

    $./stsw_rtsp_port

stsw_rtsp_port also support many command line argument, you can type the following 
command to list them: 

    $./stsw_rtsp_port --help

You can send SIGINT/SIGTERM signal to the running process to terminate the server. 
Also, you can make use of Ctrl+C in the console running stsw_rtsp_port to 
terminate it. 

   

              