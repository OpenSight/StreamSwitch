AUTOMAKE_OPTIONS=foreign 

AM_CPPFLAGS = -I$(srcdir)/../include 
AM_CXXFLAGS = -ggdb $(zeromq_CFLAGS) $(protobuf_CFLAGS)
AM_LDFLAGS = $(zeromq_LIBS) $(protobuf_LIBS) 


bin_PROGRAMS = api_test_sink file_live_source rotate_logger_test text_sink

api_test_sink_SOURCES = api_test_sink.cc
api_test_sink_LDADD = $(builddir)/../libstreamswitch.la

file_live_source_SOURCES = file_live_source.cc
file_live_source_LDADD = $(builddir)/../libstreamswitch.la

rotate_logger_test_SOURCES = rotate_logger_test.cc   
rotate_logger_test_LDADD = $(builddir)/../libstreamswitch.la

text_sink_SOURCES = text_sink.cc                          
text_sink_LDADD = $(builddir)/../libstreamswitch.la
