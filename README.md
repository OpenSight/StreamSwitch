# 1 Introduction
------------------------------------

## 1.1 What's StreamSwitch

StreamSwitch is an open source framework(library) of streaming media server 
for scalable and multi-protocol network environment.

Generally speaking, a live media stream as well as a replay stream can be 
inputted to StreamSwitch, from a specific source like a file, a device, a 
media server by different approach. StreamSwitch converts it into the internal 
format, and output it to different clients by different protocols. 
The new input or output approach (protocol) can be added to 
StreamSwitch by the plug-in method, in theory, StreamSwitch can support any 
stream media protocol as long as you write an extension(plug-in) for it. 

In StreamSwitch, the component implementing the input protocol is called 
"source", which is a standalone executable program in system. A running 
instance (i.e. an OS process) of this "source" program would get a specific 
media stream by the corresponding protocol into StreamSwitch. 
The component implementing an output protocol is called "port", which is 
also a standalone program in system. It usually works as a server of 
the corresponding protocol. When a client connects to this server, 
a child process or thread would be created, which is responsible to get 
the specified media stream from the corresponding source, and transmit to the 
client by its protocol. 
The component to manage all the "source"s and "port"s is called "controller", 
which is a (python) library, providing the API of StreamSwitch to the outside 
world. Also, the "controller" is shipped with a web application, which 
provides a Web UI for demonstrating the functionality of StreamSwitch 
to the customer. 

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

So this is StreamSwitch comes from. It's a stream protocol conversion framework 
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

# 2 Install
------------------------------------
  

# 3 Usage
------------------------------------  
  
  
# 4 Development
------------------------------------  

StreamSwitch make use of Github to host its latest code with the following URL: 

    https://github.com/OpenSight/StreamSwitch

Developers should use the issue system of Github to feedback some 
bug/requirement to StreamSwitch

StreamSwitch adopt an git work-flow similar with the popular github-flow 
(http://githubflow.github.io/). If Developers want to participate in 
StreamSwitch's development and contribute their code to StreamSwitch repository, 
they should make use of the Pull Request mechanism against StreamSwitch's master 
branch as github-flow. If StreamSwitch adopt your code, we would put your name in 
StreamSwitch author list. Thanks for your support

The following give a brief description about some key principle in 
StreamSwitch design, to help users or developers to understand StreamSwitch 
more. 

## 4.1 Architecture 

![StreamSwitch Arch](https://github.com/OpenSight/StreamSwitch/wiki/images/arch.png)

The main components in StreamSwitch is called "Source" and "Port". 
"Source" implements the input protocol. 
One kind of protocol is implemented by one kind of source, for example, RTSP 
Source implements RTSP protocol, RTMP Source implements RTMP protocol, etc. 
Each running instance (i.e. an OS process) of the source get a specific 
media stream from outside by its corresponding protocol, and publish it 
through the StreamSwitch protocol. So, a running source instance can represent 
the media stream inputted by it. A media stream in StreamSwitch also means the 
running source instance associated with it. No difference between the media 
stream and the running source instance. 

Each media stream has a unique name in StreamSwitch to identify it. 
Other component can subscribe a certain media stream through StreamSwitch 
protocol according to its stream name. 

The "Port" in StreamSwitch is to implement an output protocol. Usually, 
the port works as the server of its corresponding protocol. When a client 
connect to this port server, it would create a child process or thread to deal 
with this client. This process or thread normally will subscribe one certain 
stream according to the stream name which is often given in the request URL, 
and transmit to the client by its protocol. Multi clients (processes) can 
subscribe the same media stream at the same time. 


## 4.2 Protocol

The key of StreamSwitch is "protocol". StreamSwitch Protocol is composed of 
two parts, serialization and transport. Serialization part is responsible 
to pack the message, and transport part is to transfer the data between 
different components. 

### 4.2.1 Serialization

StreamSwitch make use of the Protobuf library to implements its serialization. 
Protobuf is a famous open source library for data serialization from Google, 
which is more efficient (i.e. producing shorter data string) and faster than 
JSON, and compatible with the binary data type which JSON not.  Last but not 
least, it's stable and easy to use.

In StreamSwitch, the basic communication unit is called "Packet", which can be 
divided into the following three types:

1. Request
2. Reply
3. Message

A request packet must be responded with a corresponding reply packet whit the 
same sequence number. And a message packet is sent alone which is used for 
broadcast. 

Each packet is composed of two parts, the header and the body. The header has 
the same format for all kinds of the packets, even though some fields of it 
would be ineffective for some type. The content of the body depends on the 
type and operation code fields in packet header. Different data would be 
placed into the body for different type and operation code. 

### 4.2.2 Transport

StreamSwitch make use of the ZeroMQ library to send/receive the packets 
to/from the other components. 
ZeroMQ is known as the fastest message queue in the open source world, 
which supports multi kinds of communication pattern, like request-reply, 
sub-pub, pipeline, and etc., and multi transport techniques, include TCP, IPC. 

StreamSwitch uses two type of ZeroMQ sockets to meet different requirement. 
Request-Reply socket is used to implements RPC communication between Source 
and Sink, and Pub-Sub socket is used to publish the packets from Source to 
sink. Moreover, StreamSwitch also make use of multi-part message feature 
of ZeroMQ. 

The content of the multi-part message on the request-reply socket is 
defined as below: 

| packet | extra blob |

First part of the message contains the data of an above "packet".
Second part of the message (optional) can contains the extra blob data 
related this "packet"


The content of the multi-part message carried on the pub-sub socket is 
defined as below: 

| channel name | packet | extra blob |

First part of the message contains a string identifying the publish channel. 
Second part of the message contains the data of an above "packet".
Third part of the message (optional) can contains the extra blob data 
related this "packet"
