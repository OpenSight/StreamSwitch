# 1 StreamSwitch Introduction
------------------------------------

## 1.1 What's StreamSwitch

StreamSwitch is an opensource framework(library) of streaming media server for for scalable and multi-protocol network environment.

Generally speaking, StreamSwitch can inputs a live media stream as well as a replay stream, from a specific source like a file, a device, a media server by different approach, convert it into the internal format, and output to diffrent clients by different protocols. The new input or output approach (protocol) can be added to StreamSwitch by the plugin method, in theory, StreamSwitch can support any stream media protocol as long as you write an extension(plugin) for it. 

In StreamSwitch, the component implenting an input protocol is called "source", which is a standalone executable program in system. Each of the input media streams based on this protocol is one running instance (i.e. OS Process) of this "source" program. The component implenting an output protocol is called "port", which is also a standalone program in system. The main process of this "port" program is usally responsible to master the server of this protocol, like listening on the socket, accpeting the incoming connections. Each of the output stream to one client is usually implemented as a forked child process or thread of this "port". The component to manage all the "source"s and "port"s is called "controller", which is a (python) library, providing operation interfaces of StreamSwitch to the outside world. Also, the "controller" is shipped with a web application, which provides a Web UI for demostrating the functionality of StreamSwitch to the customer. 

From the above, it's very easy to add the new input/output protocol support to StreamSwitch. And each protcol component in StreamSwitch is completely independent, and each transmitting media stream on StreamSwitch can work totally in parallel. On the other hand, because each of the StreamSwitch component is a standalone program, which communicates with each other throught network, the customer can deloy all of them on the same mahine, as well as deloy different components on different node. 

Not only that, StreamSwitch also provide a special source, called "proxy" source, which is the secret weapon to construct a cascade streaming media server cluster to deal with the massive media streams.


## 1.2 Why need StreamSwitch

In the past time, streaming media server is usualy focus on one protocol, like LIVE555, Red5. They are very excellent to handle its own protocol. But in pratice, there are many kind of streaming protocol active in the world, like RTSP, RTMP, HLS, and various private protocols of manufactures. In many case, the media source output the media stream on one protocol, but your clients can only receive the mediay by another protocol, even different clients support different protocols. The solution for these situation is converting, which is also a nighmare for us. To rescue us from this bad circumstance, we decide to develop a new stream server framework to support all kinds of conversion between protocols. All Input protcol is converted to the intermedia form, and all kinds of output protocol is converted from the intermedia form, so that the input and output protocol can decouple. Engineer can focus on one specific protcol processing which he is familiar with, and does not need to need how to convert with the other protocols. 

So this is StreamStream comes from. It's a stream protocol conversion framework rather than a real media server, and all the "source"s and "port"s in it are the pratical media server based on this framework. 

StreamSwitch shipped with some "source"/"ports" for the popular streaming protocol, and the new "source" / "ports" would continually be added in as time goes on. The user can also develop his "source" / "port" for his private protocol to make use the conversion power of StreamSwitch. 


## 1.3 Highlights

* Multi Protocol. Various stream media protocol is supported by StreamSwitchfor fan-in and fan-out, including RTP/RTSP, RTMP, HLS and etc.
* Extensible. User can develop a standalone extension to add other stream protocol. 
* Componentization. Each input and output protocol is designed as an independent component (e.g. a independent program), without interference. 
* Distributed Service. User can deloy all its protocol component in one physical machine as well as deloy different components on multi physical nodes.
* Parallelization. Each input and output stream is implemented as an independent process, which can run concurrently on the modern multi-core computer
* Massive scale. Through the "proxy" source, it's easy to construct a cascade media delivery cluster to service massive media streams simultaneously. 
* Recordable. StreamSwitch support recording the media stream on the modern filesystem like ext4, xfs as a mp4 fix-length file 
