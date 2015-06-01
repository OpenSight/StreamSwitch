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

As a "feng" derivative, it depends the following library (open source): 

1. libev >= 4.0
2. libnetembryo >= 0.1.0
3. glib-2.0 >= 2.16
