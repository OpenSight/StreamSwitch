AUTOMAKE_OPTIONS=foreign subdir-objects

AM_CPPFLAGS = -I$(srcdir)/../../libstreamswitch/include \
    -I$(srcdir)/src 

AM_CFLAGS = $(libnetembryo_CFLAGS) $(glib_CFLAGS) $(zeromq_CFLAGS) $(protobuf_CFLAGS)    
AM_CXXFLAGS = $(libnetembryo_CFLAGS) $(glib_CFLAGS) $(zeromq_CFLAGS) $(protobuf_CFLAGS)
AM_LDFLAGS = $(libnetembryo_LIBS) -lev $(glib_LIBS)  $(zeromq_LIBS) $(protobuf_LIBS) 

bin_PROGRAMS = stsw_rtsp_port

stsw_rtsp_port_SOURCES =  \
	src/bufferqueue.c src/bufferqueue.h \
	src/fnc_log.cc src/fnc_log.h \
	src/incoming.c src/incoming.h \
	src/feng.h \
	src/feng_utils.h \
	src/main.cc \
    src/config.h \
    src/parse_args.cc  \
    \
	src/conf/array.c \
	src/conf/array.h \
	src/conf/buffer.c \
	src/conf/buffer.h \
	src/conf/conf.h \
	src/conf/data_array.c \
	src/conf/data_count.c \
	src/conf/data_integer.c \
	src/conf/data_config.c \
	src/conf/data_string.c \
	src/conf/settings.h \
	\
	src/liberis/headers.h \
	src/liberis/utils.c liberis/utils.h \
	src/liberis/headers_parser.c \
	\
	src/network/ragel_request_line.c \
	src/network/ragel_transport.c \
	src/network/ragel_range.c \
	src/network/ragel_parsers.h \
	src/network/rtcp.c \
	src/network/rtp.c network/rtp.h \
	src/network/rtp_port.c \
	src/network/rtsp.h \
	src/network/rtsp_client.c \
	src/network/rtsp_interleaved.c \
	src/network/rtsp_lowlevel.c \
	src/network/rtsp_method_describe.c \
	src/network/rtsp_method_options.c \
	src/network/rtsp_method_pause.c \
	src/network/rtsp_method_play.c \
	src/network/rtsp_method_setup.c \
	src/network/rtsp_method_teardown.c \
	src/network/rtsp_method_get_parameter.c \
	src/network/rtsp_response.c \
	src/network/rtsp_state_machine.c \
	src/network/rtsp_utils.c \
	\
	src/media/demuxer.c \
	src/media/demuxer.h \
	src/media/demuxer_module.h \
	src/media/mediaparser.c \
	src/media/mediaparser.h \
	src/media/mediaparser_module.h \
	src/media/mediautils.c \
	src/media/resource.c \
	src/media/demuxer/demuxer_stsw.cc \
    src/media/parser/h264.c \
	src/media/parser/vorbis.c \
	src/media/parser/theora.c \
	src/media/parser/aac.c \
	src/media/parser/mp4ves.c \
	src/media/parser/speex.c \
	src/media/parser/amr.c \
	src/media/parser/pcmu.c \
	src/media/parser/pcma.c \
	src/media/parser/simple.c \
    src/media/parser/mp2p.cc  \
    src/media/parser/put_bits.h     


    
    
stsw_rtsp_port_LDADD = $(builddir)/../../libstreamswitch/libstreamswitch.la 


