"""
streamswitch.wsgiapp.services.stream_service
~~~~~~~~~~~~~~~~~~~~~~~

This module implements the service class for stream management

:copyright: (c) 2015 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""
from __future__ import unicode_literals, division
from ...exceptions import StreamSwitchError
from gevent.event import Event
from ...events import StreamInfoEvent
import gevent
from ...process_mngr import kill_all
from ...sources.file_live_source import FILE_LIVE_SOURCE_PROGRAM_NAME
from ...sources.proxy_source import PROXY_SOURCE_PROGRAM_NAME
from ...sources.rtsp_source import RTSP_SOURCE_PROGRAM_NAME
from ..models import StreamConf


class StreamInfoNotifier(object):

    MAX_WAITER = 1000

    def __init__(self, stream_name):
        self.stream_name = stream_name
        self.stream_info = None
        self.ready_event = Event()
        self.cur_waiter_num = 0

    def wait_stream_info(self, timeout):
        if self.cur_waiter_num > self.MAX_WAITER:
            raise StreamSwitchError("Too Many Waiter for Stream Info (%s)" %
                                    self.stream_name, 503)
        self.cur_waiter_num += 1

        if not self.ready_event.wait(timeout=timeout):
            # timeout
            raise StreamSwitchError(
                "Stream Info (%s) Not Ready" % self.stream_name, 408)

        return self.stream_info

    def put_stream_info(self, stream_info):
        self.stream_info = stream_info
        self.ready_event.set()


class StreamService(object):

    def __init__(self, stream_mngr=None, stream_conf_dao=None, dao_context_mngr=None):
        self._stream_mngr = stream_mngr
        self._stream_conf_dao = stream_conf_dao
        self._dao_context_mngr = dao_context_mngr
        self._stream_info_notifier = {}
        self._stream_event_listener = self.on_stream_event
        self._stream_map = {}
        self._stream_list = []

    def on_application_created(self, event):
        # print("stream service ........load")
        kill_all(RTSP_SOURCE_PROGRAM_NAME)
        kill_all(PROXY_SOURCE_PROGRAM_NAME)
        kill_all(FILE_LIVE_SOURCE_PROGRAM_NAME)
        self.load()

    def load(self):
        if self._stream_conf_dao is None \
            or self._dao_context_mngr is None:
            return

        with self._dao_context_mngr.context():
            stream_conf_list = self._stream_conf_dao.get_all_stream_conf()

        supported_source_types = self._stream_mngr.list_source_types()
        for stream_conf in stream_conf_list:
            if stream_conf.source_type in supported_source_types:
                stream = self._stream_mngr.create_stream(
                    source_type=stream_conf.source_type,
                    stream_name=stream_conf.stream_name,
                    url=stream_conf.url,
                    api_tcp_port=stream_conf.api_tcp_port,
                    log_file=stream_conf.log_file,
                    log_size=stream_conf.log_size,
                    log_rotate=stream_conf.log_rotate,
                    err_restart_interval=stream_conf.err_restart_interval,
                    extra_options=stream_conf.extra_options,
                    event_listener=self._stream_event_listener,
                    **stream_conf.other_kwargs)
                self._stream_map[stream_conf.stream_name] = stream
                self._stream_list.append(stream)


    def on_stream_event(self, event):
        # print("on stream event")
        if isinstance(event, StreamInfoEvent):
            notifier = self._stream_info_notifier.pop(event.stream.stream_name, None)
            if notifier is not None:
                notifier.put_stream_info(event.stream_info)

    def get_stream_list(self):
        return list(self._stream_list)

    def get_stream(self, stream_name):
        stream = self._stream_map.get(stream_name)
        if stream is None:
            raise StreamSwitchError(
                "Stream (%s) Not Found" % stream_name, 404)
        return stream

    def del_stream(self, stream_name):
        with self._dao_context_mngr.context():
            stream = self._stream_map.get(stream_name)
            if stream is None:
                raise StreamSwitchError(
                    "Stream (%s) Not Found" % stream_name, 404)
            del self._stream_map[stream_name]
            try:
                self._stream_list.remove(stream)
            except ValueError:
                pass
            self._stream_info_notifier.pop(stream_name, None)

            stream.destroy()   # may wait

            self._stream_conf_dao.del_stream_conf(stream_name)

    def new_stream(self, stream_configs):
        with self._dao_context_mngr.context():

            stream_conf = StreamConf(**stream_configs)
            self._stream_conf_dao.add_stream_conf(stream_conf)

            stream = \
                self._stream_mngr.create_stream(event_listener=self._stream_event_listener,
                                               **stream_configs)
            self._stream_map[stream_conf.stream_name] = stream
            self._stream_list.append(stream)

        return stream

    def get_source_type_list(self):
        return self._stream_mngr.list_source_types()

    def restart_stream(self, stream_name):
        stream = self._stream_map.get(stream_name)
        if stream is None:
            raise \
                StreamSwitchError( "Stream (%s) Not Found" % stream_name, 404)
        stream.restart()

    def get_stream_metadata(self, stream_name, timeout=5.0):
        stream = self._stream_map.get(stream_name)
        if stream is None:
            raise StreamSwitchError(
                "Stream (%s) Not Found" % stream_name,
                404)
        return stream.get_stream_metadata(timeout=timeout)

    def get_stream_statistic(self, stream_name, timeout=5.0):
        stream = self._stream_map.get(stream_name)
        if stream is None:
            raise StreamSwitchError(
                "Stream (%s) Not Found" % stream_name,
                404)
        return stream.get_stream_statistic(timeout=timeout)

    def key_frame(self, stream_name, timeout=5.0):
        stream = self._stream_map.get(stream_name)
        if stream is None:
            raise StreamSwitchError(
                "Stream (%s) Not Found" % stream_name,
                404)
        stream.key_frame(timeout=timeout)

    def get_stream_clients(self, stream_name, timeout=5.0):
        stream = self._stream_map.get(stream_name)
        if stream is None:
            raise StreamSwitchError(
                "Stream (%s) Not Found" % stream_name,
                404)
        return stream.get_client_list(start_index=0,
                                       max_client_num=100000,
                                       timeout=timeout).client_list

    def wait_subsequent_stream_info(self, stream_name, timeout=5.0):
        stream = self._stream_map.get(stream_name)
        if stream is None:
            raise StreamSwitchError(
                "Stream (%s) Not Found" % stream_name,
                404)

        notifier = self._stream_info_notifier.get(stream_name)
        if notifier is None:
            notifier = StreamInfoNotifier(stream_name)
            self._stream_info_notifier[stream_name] = notifier

        return notifier.wait_stream_info(timeout)