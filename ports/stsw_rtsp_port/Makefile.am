AUTOMAKE_OPTIONS=foreign subdir-objects

AM_CPPFLAGS = -I$(srcdir)/../../libstreamswitch/include \
    -I$(srcdir)/src 

AM_CFLAGS = $(libnetembryo_CFLAGS) $(glib_CFLAGS) $(zeromq_CFLAGS) $(protobuf_CFLAGS)    
AM_CXXFLAGS = $(libnetembryo_CFLAGS) $(glib_CFLAGS) $(zeromq_CFLAGS) $(protobuf_CFLAGS)
AM_LDFLAGS = $(libnetembryo_LIBS) -lev $(glib_LIBS)  $(zeromq_LIBS) $(protobuf_LIBS) 

bin_PROGRAMS = stsw_rtsp_port

stsw_rtsp_port_SOURCES =  \
	bufferqueue.c bufferqueue.h \
	fnc_log.cc fnc_log.h \
	incoming.c incoming.h \
	feng.h \
	feng_utils.h \
	main.cc \
    config.h \
    \
	conf/array.c \
	conf/array.h \
	conf/buffer.c \
	conf/buffer.h \
	conf/conf.h \
	conf/data_array.c \
	conf/data_count.c \
	conf/data_integer.c \
	conf/data_config.c \
	conf/data_string.c \
	conf/settings.h \
	\
	liberis/headers.h \
	liberis/utils.c liberis/utils.h \
	liberis/headers_parser.c \
	\
	network/ragel_request_line.c \
	network/ragel_transport.c \
	network/ragel_range.c \
	network/ragel_parsers.h \
	network/rtcp.c \
	network/rtp.c network/rtp.h \
	network/rtp_port.c \
	network/rtsp.h \
	network/rtsp_client.c \
	network/rtsp_interleaved.c \
	network/rtsp_lowlevel.c \
	network/rtsp_method_describe.c \
	network/rtsp_method_options.c \
	network/rtsp_method_pause.c \
	network/rtsp_method_play.c \
	network/rtsp_method_setup.c \
	network/rtsp_method_teardown.c \
	network/rtsp_method_get_parameter.c \
	network/rtsp_response.c \
	network/rtsp_state_machine.c \
	network/rtsp_utils.c \
	\
	media/demuxer.c \
	media/demuxer.h \
	media/demuxer_module.h \
	media/mediaparser.c \
	media/mediaparser.h \
	media/mediaparser_module.h \
	media/mediautils.c \
	media/resource.c \
	media/demuxer/demuxer_stsw.c \
    media/parser/h264.c \
	media/parser/vorbis.c \
	media/parser/theora.c \
	media/parser/aac.c \
	media/parser/mp4ves.c \
	media/parser/speex.c \
	media/parser/amr.c \
	media/parser/pcmu.c \
	media/parser/pcma.c \
	media/parser/simple.c 


    
    
stsw_rtsp_port_LDADD = $(builddir)/../../libstreamswitch/libstreamswitch.la 


