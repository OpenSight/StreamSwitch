AUTOMAKE_OPTIONS=foreign subdir-objects

AM_CPPFLAGS = -I$(srcdir)/../../libstreamswitch/include -D__STDC_CONSTANT_MACROS 
AM_CXXFLAGS = $(zeromq_CFLAGS) $(protobuf_CFLAGS) $(libavformat_CFLAGS) $(libavutil_CFLAGS) $(libcurl_CFLAGS)
AM_LDFLAGS = $(zeromq_LIBS) $(protobuf_LIBS) $(libavformat_LIBS) $(libavutil_LIBS) $(libcurl_LIBS)


bin_PROGRAMS = ffmpeg_sender

ffmpeg_sender_SOURCES = src/stsw_main.cc \
    src/stsw_ffmpeg_sender_arg_parser.cc \
    src/stsw_ffmpeg_sender_arg_parser.h \
    src/stsw_ffmpeg_muxer_sender.cc \
    src/stsw_ffmpeg_muxer_sender.h \
    src/stsw_ffmpeg_muxer.cc \
    src/stsw_ffmpeg_muxer.h \
    src/stsw_cached_segment.c \
    src/stsw_cached_segment.h \
    src/cJSON.c \
    src/cJSON.h \
    src/seg_writers/cseg_dummy_writer.c \
    src/seg_writers/cseg_file_writer.c \
    src/seg_writers/cseg_ivr_writer.c \
    src/parsers/stsw_stream_mux_parser.cc \
    src/parsers/stsw_stream_mux_parser.h \
    src/parsers/stsw_h264or5_mux_parser.cc \
    src/parsers/stsw_h264or5_mux_parser.h \
    src/parsers/stsw_mpeg4_video_mux_parser.cc \
    src/parsers/stsw_mpeg4_video_mux_parser.h \
    src/parsers/stsw_aac_mux_parser.cc \
    src/parsers/stsw_aac_mux_parser.h \
    src/parsers/dec_sps.cc \
    src/parsers/dec_sps.h \
    src/stsw_ffmpeg_sender_global.h \
    src/stsw_log.cc \
    src/stsw_log.h
    

ffmpeg_sender_LDADD = $(builddir)/../../libstreamswitch/libstreamswitch.la

