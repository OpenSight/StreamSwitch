AUTOMAKE_OPTIONS=foreign subdir-objects

AM_CPPFLAGS = -I$(srcdir)/../../libstreamswitch/include 
AM_CXXFLAGS = $(zeromq_CFLAGS) $(protobuf_CFLAGS) $(librtmp_CFLAGS) 
AM_LDFLAGS = $(zeromq_LIBS) $(protobuf_LIBS) $(librtmp_LIBS)


bin_PROGRAMS = stsw_rtmp_source 

stsw_rtmp_source_SOURCES = src/stsw_rtmp_main.cc \
    src/stsw_rtmp_source.cc   \
    src/stsw_rtmp_source.h
    
stsw_rtmp_source_LDADD = $(builddir)/../../libstreamswitch/libstreamswitch.la


                        
