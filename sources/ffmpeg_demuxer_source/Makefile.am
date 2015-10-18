AUTOMAKE_OPTIONS=foreign subdir-objects

AM_CPPFLAGS = -I$(srcdir)/../../libstreamswitch/include  
AM_CXXFLAGS = $(zeromq_CFLAGS) $(protobuf_CFLAGS)
AM_LDFLAGS = $(zeromq_LIBS) $(protobuf_LIBS) 


bin_PROGRAMS = stsw_proxy_source

stsw_proxy_source_SOURCES = src/stsw_main.cc \
    src/stsw_stream_proxy.cc \
    src/stsw_stream_proxy.h \
    src/url.h \
    src/stsw_log.cc \
    src/stsw_log.h

stsw_proxy_source_LDADD = $(builddir)/../../libstreamswitch/libstreamswitch.la

