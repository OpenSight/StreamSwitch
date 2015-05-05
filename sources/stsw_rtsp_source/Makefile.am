AUTOMAKE_OPTIONS=foreign subdir-objects

AM_CPPFLAGS = -I$(srcdir)/../../libstreamswitch/include \
    -I$(srcdir)/live/BasicUsageEnvironment/include \
    -I$(srcdir)/live/groupsock/include \
    -I$(srcdir)/live/liveMedia/include \
    -I$(srcdir)/live/UsageEnvironment/include
    
AM_CXXFLAGS = $(zeromq_CFLAGS) $(protobuf_CFLAGS)
AM_LDFLAGS = $(zeromq_LIBS) $(protobuf_LIBS) 

EXTRA_DIST = live

bin_PROGRAMS = stsw_rtsp_source 

stsw_rtsp_source_SOURCES = src/stsw_main.cc \
    src/stsw_rtsp_client.cc \
    src/stsw_rtsp_client.h \
    src/stsw_pts_normalizer.cc \
    src/stsw_pts_normalizer.h \
    src/stsw_output_sink.cc \
    src/stsw_output_sink.h \
    src/stsw_mpeg4_output_sink.cc \
    src/stsw_mpeg4_output_sink.h \
    src/stsw_h264or5_output_sink.cc\
    src/stsw_h264or5_output_sink.h 
    
    
stsw_rtsp_source_LDADD = $(builddir)/../../libstreamswitch/libstreamswitch.la \
                         $(srcdir)/live/liveMedia/libliveMedia.a \
                         $(srcdir)/live/BasicUsageEnvironment/libBasicUsageEnvironment.a \
                         $(srcdir)/live/UsageEnvironment/libUsageEnvironment.a \
                         $(srcdir)/live/groupsock/libgroupsock.a

$(srcdir)/live/liveMedia/libliveMedia.a:
	cd $(srcdir)/live/liveMedia; make 

$(srcdir)/live/BasicUsageEnvironment/libBasicUsageEnvironment.a:
	cd $(srcdir)/live/BasicUsageEnvironment; make

$(srcdir)/live/UsageEnvironment/libUsageEnvironment.a: 
	cd $(srcdir)/live/UsageEnvironment; make
    
$(srcdir)/live/groupsock/libgroupsock.a:
	cd $(srcdir)/live/groupsock; make                               
                         
                         
clean-local:
	cd $(srcdir)/live;make clean

distclean-local:
	cd $(srcdir)/live;make distclean


                        
