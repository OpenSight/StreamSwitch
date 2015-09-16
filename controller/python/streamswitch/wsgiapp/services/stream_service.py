"""
streamswitch.wsgiapp.services.stream_service
~~~~~~~~~~~~~~~~~~~~~~~

This module implements the service class for stream management

:copyright: (c) 2015 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""
from __future__ import unicode_literals, division
from ...exceptions import StreamSwitchError


class StreamService(object):

    def __init__(self, stream_mngr=None, stream_dao=None, tx_ctx_factory=None):
        self.stream_mngr = stream_mngr
        self.stream_dao = stream_dao
        self.tx_ctx_factory = tx_ctx_factory

    def load(self):
        pass

    def get_stream_list(self):
        return self.stream_mngr.list_streams()

    def get_stream(self, stream_name):
        stream = self.stream_mngr.find_stream(stream_name)
        if stream is None:
            raise StreamSwitchError(
                "Stream (%s) Not Found" % stream_name,
                404)
        return stream

    def del_stream(self, stream_name):
        stream = self.stream_mngr.find_stream(stream_name)
        if stream is None:
            raise StreamSwitchError(
                "Stream (%s) Not Found" % stream_name,
                404)
        stream.destroy()

    def new_stream(self, stream_config):
        stream = \
            self.stream_mngr.create_stream(
                source_type=stream_config.source_type,
                stream_name=stream_config.stream_name,
                url=stream_config.url,
                api_tcp_port=stream_config.api_tcp_port,
                log_file=stream_config.log_file,
                log_size=stream_config.log_size,
                log_rotate=stream_config.log_rotate,
                err_restart_interval=stream_config.err_restart_interval,
                extra_options=stream_config.extra_options,
                event_listener=None)
        return stream

    def get_source_type_list(self):
        return self.stream_mngr.list_source_types()

    def restart_stream(self, stream_name):
        stream = self.stream_mngr.find_stream(stream_name)
        if stream is None:
            raise StreamSwitchError(
                "Stream (%s) Not Found" % stream_name,
                404)
        stream.restart()

    def get_stream_metadata(self, stream_name, timeout=5.0):
        stream = self.stream_mngr.find_stream(stream_name)
        if stream is None:
            raise StreamSwitchError(
                "Stream (%s) Not Found" % stream_name,
                404)
        return stream.get_stream_metadata(timeout=timeout)

    def get_stream_statistic(self, stream_name, timeout=5.0):
        stream = self.stream_mngr.find_stream(stream_name)
        if stream is None:
            raise StreamSwitchError(
                "Stream (%s) Not Found" % stream_name,
                404)
        return stream.get_stream_statistic(timeout=timeout)

    def key_frame(self, stream_name, timeout=5.0):
        stream = self.stream_mngr.find_stream(stream_name)
        if stream is None:
            raise StreamSwitchError(
                "Stream (%s) Not Found" % stream_name,
                404)
        stream.key_frame(timeout=timeout)

    def get_stream_clients(self, stream_name, timeout=5.0):
        stream = self.stream_mngr.find_stream(stream_name)
        if stream is None:
            raise StreamSwitchError(
                "Stream (%s) Not Found" % stream_name,
                404)
        return stream.get_client_list(start_index=0,
                                       max_client_num=100000,
                                       timeout=timeout).client_list