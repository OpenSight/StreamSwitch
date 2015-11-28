AUTOMAKE_OPTIONS=foreign
ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = License README.md DEPENDS controller ports recorders sources

SUBDIRS = libstreamswitch

if INCLUDE_RTSP_SOURCE
SUBDIRS += sources/stsw_rtsp_source
endif

if INCLUDE_RTMP_SOURCE
SUBDIRS += sources/stsw_rtmp_source
endif 

if INCLUDE_PROXY_SOURCE
SUBDIRS += sources/stsw_proxy_source
endif 

if INCLUDE_RTSP_PORT
SUBDIRS += ports/stsw_rtsp_port
endif 

if INCLUDE_FFMPEG_DEMUXER_SOURCE
SUBDIRS += sources/ffmpeg_demuxer_source
endif

if INCLUDE_FFMPEG_SENDER
SUBDIRS += senders/ffmpeg_sender
endif