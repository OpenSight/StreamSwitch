"""
streamswitch.wsgiapp.domain.stream_config
~~~~~~~~~~~~~~~~~~~~~~~

This module implements the StreamConfig class which
is to store the config information about a stream

:copyright: (c) 2015 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""
from __future__ import unicode_literals, division

from streamswitch.stream_mngr import DEFAULT_LOG_SIZE
from streamswitch.stream_mngr import DEFAULT_LOG_ROTATE


class StreamConfig(object):
    def __init__(self, stream_name, source_type="unknown", url="", api_tcp_port=0,
                 log_file=None, log_size=DEFAULT_LOG_SIZE, log_rotate=DEFAULT_LOG_ROTATE,
                 err_restart_interval=30.0, extra_options={}, **kwargs):
        self.stream_name = stream_name
        self.source_type = source_type
        self.url = url
        self.api_tcp_port = api_tcp_port
        self.log_file = log_file
        self.log_size = log_size
        self.log_rotate = log_rotate
        self.err_restart_interval = err_restart_interval
        self.extra_options = extra_options
        self.other_params = kwargs
