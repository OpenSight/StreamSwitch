AUTOMAKE_OPTIONS=foreign subdir-objects
AM_CPPFLAGS = -ggdb -I./include -I./src/pb

EXTRA_DIST = protocol


SUBDIRS = . samples

lib_LIBRARIES=libstreamswitch.a
libstreamswitch_a_SOURCES =  src/stsw_arg_parser.cc  src/stsw_global.cc src/stsw_rotate_logger.cc src/stsw_stream_sink.cc \
                             src/stsw_stream_source.cc src/pb/pb_client_heartbeat.pb.cc src/pb/pb_client_list.pb.cc \
                             src/pb/pb_media.pb.cc src/pb/pb_media_statistic.pb.cc src/pb/pb_metadata.pb.cc src/pb/pb_packet.pb.cc \
                             src/pb/pb_stream_info.pb.cc
                    

                    
INSTALL_INCLUDE_DIR = include                    
INSTALL_INCLUDE_FILES = $(foreach d,$(INSTALL_INCLUDE_DIR),$(wildcard $(d)/*.h))

                    
include_HEADERS = $(INSTALL_INCLUDE_FILES)
