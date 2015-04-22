AUTOMAKE_OPTIONS=foreign
ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = License README.md DEPENDS controller ports recorders sources

SUBDIRS = libstreamswitch
if INCLUDE_RTSP_SOURCE
SUBDIRS += sources/stsw_rtsp_source
SUBDIRS += sources/stsw_rtmp_source
endif
 