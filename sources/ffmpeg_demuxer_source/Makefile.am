AUTOMAKE_OPTIONS=foreign subdir-objects

AM_CPPFLAGS = -I$(srcdir)/../../libstreamswitch/include -D__STDC_CONSTANT_MACROS 
AM_CXXFLAGS = $(zeromq_CFLAGS) $(protobuf_CFLAGS) $(libavformat_CFLAGS) $(libavutil_CFLAGS) 
AM_LDFLAGS = $(zeromq_LIBS) $(protobuf_LIBS) $(libavformat_LIBS) $(libavutil_LIBS) 


bin_PROGRAMS = ffmpeg_demuxer_source

ffmpeg_demuxer_source_SOURCES = src/stsw_main.cc \
    src/stsw_ffmpeg_arg_parser.cc \
    src/stsw_ffmpeg_arg_parser.h \
    src/stsw_ffmpeg_demuxer_source.cc \
    src/stsw_ffmpeg_demuxer_source.h \
    src/stsw_ffmpeg_demuxer.cc \
    src/stsw_ffmpeg_demuxer.h \
    src/stsw_ffmpeg_source_global.h \
    src/stsw_log.cc \
    src/stsw_log.h
    

ffmpeg_demuxer_source_LDADD = $(builddir)/../../libstreamswitch/libstreamswitch.la

