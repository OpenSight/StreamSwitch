"""
streamswitch.wsgiapp.services.stream_service
~~~~~~~~~~~~~~~~~~~~~~~

This module implements the service class for stream management

:copyright: (c) 2015 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""
from __future__ import unicode_literals, division
from ...exceptions import StreamSwitchError
from gevent.event import AsyncResult
from ...events import StreamInfoEvent
import gevent
from ...process_mngr import kill_all
from ...sources.file_live_source import FILE_LIVE_SOURCE_PROGRAM_NAME
from ...sources.proxy_source import PROXY_SOURCE_PROGRAM_NAME
from ...sources.rtsp_source import RTSP_SOURCE_PROGRAM_NAME

class StreamInfoNotifier(object):

    MAX_WAITER = 1000

    def __init__(self, stream_name):
        self.waiters = set()
        self.stream_name = stream_name

    def wait_stream_info(self, timeout):
        if len(self.waiters) > self.MAX_WAITER:
            raise StreamSwitchError("Too Many Waiter for Stream Info (%s)" %
                                    self.stream_name, 503)
        async_result = AsyncResult()
        self.waiters.add(async_result)
        try:
            stream_info = async_result.get(timeout=timeout)
        except gevent.timeout.Timeout:
            raise StreamSwitchError(
                "Stream Info (%s) Not Ready" % self.stream_name, 408)
        finally:
            self.waiters.discard(async_result)

        # print("return:" + str(stream_info))
        return stream_info

    def put_stream_info(self, stream_info):
        for waiter in self.waiters:
            waiter.set(stream_info)


class StreamService(object):

    def __init__(self, stream_mngr=None, stream_dao=None, tx_ctx_factory=None):
        self.stream_mngr = stream_mngr
        self.stream_dao = stream_dao
        self.tx_ctx_factory = tx_ctx_factory
        self._stream_info_notifier = {}

    def on_application_created(self, event):
        kill_all(RTSP_SOURCE_PROGRAM_NAME)
        kill_all(PROXY_SOURCE_PROGRAM_NAME)
        kill_all(FILE_LIVE_SOURCE_PROGRAM_NAME)
        self.load()

    def load(self):
        pass

    def on_stream_event(self, event):
        #print("on stream event")
        if isinstance(event, StreamInfoEvent):
            notifier = self._stream_info_notifier.pop(event.stream.stream_name, None)
            if notifier is not None:
                notifier.put_stream_info(event.stream_info)

    def get_stream_list(self):
        return self.stream_mngr.list_streams()

    def get_stream(self, stream_name):
        stream = self.stream_mngr.find_stream(stream_name)
        if stream is None:
            raise StreamSwitchError(
                "Stream (%s) Not Found" % stream_name, 404)
        return stream

    def del_stream(self, stream_name):
        stream = self.stream_mngr.find_stream(stream_name)
        if stream is None:
            raise StreamSwitchError(
                "Stream (%s) Not Found" % stream_name, 404)
        stream.destroy()
        self._stream_info_notifier.pop(stream_name, None)

    def new_stream(self, stream_configs):
        stream = \
            self.stream_mngr.create_stream(**stream_configs)
        return stream

    def get_source_type_list(self):
        return self.stream_mngr.list_source_types()

    def restart_stream(self, stream_name):
        stream = self.stream_mngr.find_stream(stream_name)
        if stream is None:
            raise \
                StreamSwitchError( "Stream (%s) Not Found" % stream_name, 404)
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

    def wait_subsequent_stream_info(self, stream_name, timeout=5.0):
        stream = self.stream_mngr.find_stream(stream_name)
        if stream is None:
            raise StreamSwitchError(
                "Stream (%s) Not Found" % stream_name,
                404)

        notifier = self._stream_info_notifier.get(stream_name)
        if notifier is None:
            notifier = StreamInfoNotifier(stream_name)
            self._stream_info_notifier[stream_name] = notifier

        return notifier.wait_stream_info(timeout)