# StreamSwitch

## What's StreamSwitch

StreamSwitch is an opensource framework(library) of stream media server for for scalable and multi-protocol network environment.

Generally speaking, StreamSwitch can inputs a live media stream as well as a replay stream, from a specific source like a file, a device, a media server by different approach, convert it into the internal format, and output to diffrent clients by different protocols. The new input or output approach (protocol) can be added to StreamSwitch by the plugin method, in theory, StreamSwitch can support all kinds of the media protocol as long as you write an extension(plugin) for it. 




## Why need StreamSwitch



## Highlights

* Multi Protocol. Various stream media protocol is supported by StreamSwitchfor fan-in and fan-out, including RTP/RTSP, RTMP, HLS and etc.
* Extensible. User can develop a standalone extension to add other stream protocol. 
* Componentization. Each input and output protocol is designed as an independent component (e.g. a independent program), without interference. 
* Distributed Service. User can deloy all its protocol component in one physical machine as well as deloy different components on multi physical nodes.
* Parallelization. Each input and output stream is implemented as an independent process, which can run concurrently on the modern multi-core computer
* recordable. StreamSwitch support recording the media stream on the modern filesystem like ext4, xfs as a mp4 fix-length file 
