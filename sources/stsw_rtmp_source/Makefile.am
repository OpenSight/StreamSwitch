AUTOMAKE_OPTIONS=foreign subdir-objects

AM_CPPFLAGS = -I$(srcdir)/../../libstreamswitch/include 
AM_CXXFLAGS = $(zeromq_CFLAGS) $(protobuf_CFLAGS)
AM_LDFLAGS = $(zeromq_LIBS) $(protobuf_LIBS) 

EXTRA_DIST = live

bin_PROGRAMS = stsw_rtmp_source 

stsw_rtmp_source_SOURCES = src/stsw_rtmp_main.cc src/stsw_rtmp_source.cc
stsw_rtmp_source_LDADD = $(builddir)/../../libstreamswitch/libstreamswitch.la -lrtmp


                        
