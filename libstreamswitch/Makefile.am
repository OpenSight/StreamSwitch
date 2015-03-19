AUTOMAKE_OPTIONS=foreign subdir-objects

AM_CPPFLAGS = -I$(srcdir)/include -I$(srcdir)/src/pb 
AM_CXXFLAGS = -ggdb $(zeromq_CFLAGS) $(protobuf_CFLAGS) 
AM_LDFLAGS = $(zeromq_LIBS) $(protobuf_LIBS) 

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libstreamswitch.pc


EXTRA_DIST = protocol


SUBDIRS = . samples

lib_LTLIBRARIES=libstreamswitch.la

libstreamswitch_la_SOURCES =  src/stsw_arg_parser.cc \
    src/stsw_global.cc \
    src/stsw_rotate_logger.cc \
    src/stsw_stream_sink.cc \
    src/stsw_stream_source.cc \
    src/pb/pb_client_heartbeat.pb.cc \
    src/pb/pb_client_heartbeat.pb.h \
    src/pb/pb_client_list.pb.cc \
    src/pb/pb_client_list.pb.h \
    src/pb/pb_media.pb.cc \
    src/pb/pb_media.pb.h \
    src/pb/pb_media_statistic.pb.cc \
    src/pb/pb_media_statistic.pb.h \
    src/pb/pb_metadata.pb.cc \
    src/pb/pb_metadata.pb.h \
    src/pb/pb_packet.pb.cc \
    src/pb/pb_packet.pb.h \
    src/pb/pb_stream_info.pb.cc \
    src/pb/pb_stream_info.pb.h 

libstreamswitch_la_LDFLAGS = -version-info 0:0:0 $(AM_LDFLAGS)   
                    
include_HEADERS = include/stream_switch.h \
    include/stsw_arg_parser.h \
    include/stsw_defs.h \
    include/stsw_global.h \
    include/stsw_lock_guard.h \
    include/stsw_rotate_logger.h \
    include/stsw_sink_listener.h \
    include/stsw_source_listener.h \
    include/stsw_stream_sink.h \
    include/stsw_stream_source.h 
