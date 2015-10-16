STSW_PROXY_SOURCE
======================

A special StreamSwitch source which is to relay a live stream from other 
StreamSwitch source.

This source works as a live stream proxy, which get the stream data from 
the other remote source and re-publish to the sinks by itself. it's used to 
construct a cascade media server cluster to deal with the massive 
transmission of the live media streams.

As you know, each server has its limitation, from its design, operation system, 
Hardware, and etc. How to service massive live stream transmission, the simple 
way is "cascade". The first level server (also called source server) get the 
specific live stream from the camera device, and transmits to the second 
level server. Then, the second level servers (also called edge server) get 
the live stream and transmit to the clients. This technique is also called 
VDN (video delivery network), which comes from CDN. Through this way, the 
number of the concurrent client for the same live video can scale out to 
million-level. stsw_proxy_source is used as the second level server. 
Someone would consider "multicast" to solve this problem. Though multicast 
is a standard of IP network, but the support for it is not well on the 
Internet. Many router cannot deal with multicast because of technology, cost, 
security, and etc. On the other hand, QOS is very low for multicast on the 
Internet. 


The design of stsw_proxy_source is simple, but it's strong and powerful. 

## STSW URL
------------------------

stsw_proxy_source make use of the private-format URL (called STSW URL) to 
access the remote live stream. STSW URL has the following format: 

    stsw://[server_ip]:[port]/[stream_name]

Where [server_ip] is the IP or domain name of the remote server running the superior source, 
[port] is the stream API tcp port of the superior source, 
[stream_name] is the name of the specific stream to receive


## How to run
----------------------

typing the following command at the STSW_PROXY_SOURCE project's root directory (the directory includes this README.md) 
can start up the stsw_proxy_source daemon at front-ground

    # ./stsw_proxy_source -s [stream_name] -u [STSW URL]

which [stream_name] is the name of the stream published by this proxy source instance, 
[STSW URL] is the STSW URL to access the remote stream. Besides above, you can get more
options by typing following command.

    #./stsw_proxy_source -h
    
You can send SIGINT/SIGTERM signal to the running process to terminate it. 
Also, you can make use of Ctrl+C in the console running stsw_proxy_source to 
terminate it.     
