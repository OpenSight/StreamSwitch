AUTOMAKE_OPTIONS=foreign subdir-objects

AM_CPPFLAGS = -I$(srcdir)/../../libstreamswitch/include 
AM_CXXFLAGS = $(zeromq_CFLAGS) $(protobuf_CFLAGS)
AM_LDFLAGS = $(zeromq_LIBS) $(protobuf_LIBS) 

EXTRA_DIST = live

bin_PROGRAMS = stsw_rtsp_source 

stsw_rtsp_source_SOURCES = src/stsw_main.cc
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


                        
