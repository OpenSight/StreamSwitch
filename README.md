# 1 Introduction
------------------------------------

## 1.1 What's StreamSwitch

StreamSwitch is an open source framework(library) of streaming media server 
for scalable and multi-protocol network environment.

Generally speaking, a live media stream as well as a 
replay stream can be input to StreamSwitch, from a specific source like a 
file, a device, a media server by different approach. StreamSwitch converts 
it into the internal format, and output it to different clients by different 
protocols. The new input or output approach (protocol) can be added to 
StreamSwitch by the plug-in method, in theory, StreamSwitch can support any 
stream media protocol as long as you write an extension(plug-in) for it. 

In StreamSwitch, the component implementing an input protocol is called 
"source", which is a standalone executable program in system. Each of the 
input media streams based on this protocol is a running instance 
(i.e. OS Process) of this "source" program. The component implementing an 
output protocol is called "port", which is also a standalone program in system. 
The main process of this "port" program is usually responsible to master the 
server of this protocol, like listening on the socket, accepting the incoming 
connections. Each of the output stream to one client is usually implemented as 
a forked child process or thread of this "port". The component to manage all 
the "source"s and "port"s is called "controller", which is a (python) library, 
providing API of StreamSwitch to the outside world. Also, the "controller" is 
shipped with a web application, which provides a Web UI for demonstrating the 
functionality of StreamSwitch to the customer. 

From the above, it's very easy to add the new input/output protocol support to 
StreamSwitch. And each protocol component in StreamSwitch is completely 
independent, and each transmitting media stream on StreamSwitch can work totally 
in parallel. On the other hand, because each component in the StreamSwitch is a 
standalone program, which communicates with each other through network, the 
customer can deploy all of them on the same machine, as well as deploy different 
components on different node. 

Not only that, StreamSwitch also provide a special source, called "proxy" source, 
which is the secret weapon to construct a cascade media server cluster 
to deal with the massive transmission of media streams.


## 1.2 Why need StreamSwitch

In the past time, streaming media server is usually focus on one certain 
protocol, like LIVE555, Red5, Darwin. They are very excellent to handle its 
own protocol. But in practice, there are many kind of streaming protocol 
active in the world, like RTSP, RTMP, HLS, and various private protocols of 
manufactures. In many case, the media source output the media stream by a 
certain protocol, but your clients can only receive the media by another 
protocol, even different clients need different protocols. The solution for 
these situation is "converting", which is also a nightmare for us. To rescue 
us from this bad circumstance, we decide to develop a new stream server 
framework to support all kinds of conversion between protocols. All Input 
protocol is converted to the intermediate form, and all kinds of output 
protocol is converted from the intermediate form, so that the input and output 
can decouple. Engineer can focus on one specific protocol processing which he 
is familiar with, and does not need to need how to convert with the other 
protocols. 

So this is StreamStream comes from. It's a stream protocol conversion framework 
rather than merely a media server, and all the "source"s and "port"s in it are 
the practical media server based on this framework. 

StreamSwitch shipped with some "source"/"ports" for the popular streaming protocol, 
and the new "source" / "ports" would continually be added in as time goes on. 
The user can also develop his "source" / "port" for his private protocol to make 
use the conversion power of StreamSwitch. 


## 1.3 Highlights

* Multi Protocol. Various stream media protocol is supported by StreamSwitch 
  for fan-in and fan-out, including RTP/RTSP, RTMP, HLS, etc.
* Extensible. User can develop a standalone extension for his stream protocol. 
* Componentization. Each input and output protocol is designed as an independent 
  component (e.g. a independent program), without interference. 
* Distributed Service. User can deploy all component of StreamSwitch in one 
  physical machine as well as deploy different components on multi physical nodes.
* Parallelization. Each input and output stream is implemented as an independent 
  process, which can run concurrently on the modern multi-core computer
* Massive scale. Through the "proxy" source, it's easy to construct a cascade media 
  delivery cluster to service massive transmission of media streams simultaneously. 
* Recordable. StreamSwitch support recording the media stream on the modern 
  FileSystem like ext4, xfs as a mp4 fix-length file 

  
  