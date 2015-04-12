# 1 StreamSwitch Introduction
------------------------------------

## 1.1 What's StreamSwitch

StreamSwitch is an opensource framework(library) of stream media server for for scalable and multi-protocol network environment.

Generally speaking, StreamSwitch can inputs a live media stream as well as a replay stream, from a specific source like a file, a device, a media server by different approach, convert it into the internal format, and output to diffrent clients by different protocols. The new input or output approach (protocol) can be added to StreamSwitch by the plugin method, in theory, StreamSwitch can support any stream media protocol as long as you write an extension(plugin) for it. 

In StreamSwitch, the component implenting an input protocol is called "source", which is a standalone executable program in system. Each of the input media streams based on this protocol is one running instance (i.e. OS Process) of this "source" program. The component implenting an output protocol is called "port", which is also a standalone program in system. The main process of this "port" program is usally responsible to master the server of this protocol, like listening on the socket, accpeting the incoming connections. Each of the output stream to one client is usually implemented as a forked child process or thread of this "port". The component to manage all the "source"s and "port"s is called "controller", which is a (python) library, providing operation interfaces of StreamSwitch to the outside world. Also, the "controller" is shipped with a web application, which provides a Web UI for demostrating the functionality of StreamSwitch to the customer. 

From the above, it's very easy to add the new input/output protocol support to StreamSwitch. And each protcol component in StreamSwitch is completely independent, and each transmitting media stream on StreamSwitch can work totally in parallel. 



## 1.2 Why need StreamSwitch



## 1.3 Highlights

* Multi Protocol. Various stream media protocol is supported by StreamSwitchfor fan-in and fan-out, including RTP/RTSP, RTMP, HLS and etc.
* Extensible. User can develop a standalone extension to add other stream protocol. 
* Componentization. Each input and output protocol is designed as an independent component (e.g. a independent program), without interference. 
* Distributed Service. User can deloy all its protocol component in one physical machine as well as deloy different components on multi physical nodes.
* Parallelization. Each input and output stream is implemented as an independent process, which can run concurrently on the modern multi-core computer
* recordable. StreamSwitch support recording the media stream on the modern filesystem like ext4, xfs as a mp4 fix-length file 
