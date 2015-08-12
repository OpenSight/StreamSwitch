"""
streamswitch.control.stream_mngr
~~~~~~~~~~~~~~~~~~~~~~~

This module implements the input stream management

:copyright: (c) 2015 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""

from __future__ import unicode_literals, division
from streamswitch.utils.exceptions import StreamSwitchError
from streamswitch.utils.events import StreamSubsriberEvent
import zmq.green as zmq
import gevent
from gevent import sleep
import sys
import os
import time

DEFAULT_LOG_SIZE = 1024 * 1024
DEFAULT_LOG_ROTATE = 3

STREAM_STATE_CONNECTING = 0
STREAM_STATE_OK = 1
# error
STREAM_STATE_ERR = -1
STREAM_STATE_ERR_CONNECT_FAIL = -2
STREAM_STATE_ERR_MEIDA_STOP = -3
STREAM_STATE_ERR_TIME = -4

PLAY_TYPE_LIVE = 0
PLAY_TYPE_REPLAY = 1


STREAM_MODE_ACTIVE = "active"
STREAM_MODE_PASSIVE = "passive"


SUB_STREAM_MEIDA_TYPE_VIDEO = 0
SUB_STREAM_MEIDA_TYPE_AUDIO = 1
SUB_STREAM_MEIDA_TYPE_TEXT = 2
SUB_STREAM_MEIDA_TYPE_PRIVATE = 3


SUB_STREAM_DIRECTION_OUTBOUND = 0
SUB_STREAM_DIRECTION_INBOUND = 1

IP_VERSION_V4 = 0
IP_VERSION_V6 = 1

STSW_SOCKET_NAME_STREAM_PREFIX = "stsw_stream"
STSW_SOCKET_NAME_STREAM_PUBLISH = "broadcast"




class SubStreamMetaData(object):
    def __init__(self):
        self.index = 0
        self.media_type = SUB_STREAM_MEIDA_TYPE_VIDEO
        self.codec_name = "h264"
        self.direction = SUB_STREAM_DIRECTION_OUTBOUND
        self.extra_data = b''

        # video params
        self.height = 0
        self.width = 0
        self.fps = 0
        self.gov = 0

        # audio params
        self.samples_per_second = 0
        self.channels = 0
        self.bits_per_sample = 0
        self.samples_per_frame = 0

        # text params
        self.x = 0
        self.y = 0
        self.font_size = 0
        self.font_type = 0


class StreamMetaData(object):
    def __init__(self):
        self.play_type = PLAY_TYPE_LIVE
        self.source_protocol = ""
        self.stream_len = 0.0
        self.ssrc = 0
        self.bps = 0
        self.sub_streams = []


class SubStreamStatistic(object):
    def __init__(self):
        self.sub_stream_index = 0
        self.media_type = SUB_STREAM_MEIDA_TYPE_VIDEO
        self.data_bytes = 0
        self.key_bytes = 0
        self.lost_frames = 0
        self.data_frames = 0
        self.key_frames = 0
        self.last_gov = 0


class StreamStatistic(object):
    def __init__(self):
        self.ssrc = 0
        self.timestamp = 0.0
        self.sum_bytes = 0
        self.sub_stream_stats = []


class ClientInfo(object):
    def __init__(self):
        self.client_ip_version = IP_VERSION_V4
        self.client_ip = "127.0.0.1"
        self.client_port = 0
        self.client_token = ""
        self.client_protocol = ""
        self.client_text = ""
        self.last_active_time = 0.0


class ClientList(object):
    def __init__(self):
        self.total_num = 0
        self.start_index = 0
        self.client_list = []


class StreamBase(object):
    def __init__(self, stream_name, manager, source_type, url, api_tcp_port=0, log_file=None, log_size=DEFAULT_LOG_SIZE,
                   log_rotate=DEFAULT_LOG_ROTATE, err_restart_interval=30, extra_args={}, event_listener=None):
        self._mngr = manager
        self._stream_name = stream_name  # stream_name cannot be modified
        self._event_listener = event_listener
        self._has_destroy = False
        self._api_client_socket = None
        self._subscriber_greenlet = None

        # config
        self.source_type = source_type
        self.url = url
        self.api_tcp_port = api_tcp_port
        self.log_file = log_file
        self.log_size = log_size
        self.log_rotate = log_rotate
        self.err_restart_interval = err_restart_interval
        self.extra_conf = extra_args
        self.mode = STREAM_MODE_ACTIVE
        self.cmd_args = []

        # status
        self.state = STREAM_STATE_CONNECTING
        self.play_type = PLAY_TYPE_LIVE
        self.source_protocol = ""
        self.ssrc = 0
        self.cur_bps = 0
        self.last_frame_time = 0.0
        self.update_time = time.time()
        self.client_num = 0

    @property
    def stream_name(self):
        return self._stream_name

    def start(self):
        if self._has_destroy:
            raise StreamSwitchError("Stream Already Destroy", 400)
        self._subscriber_greenlet = gevent.spawn(self._subscriber_run)

        try:
            self._start_internal()
        except Exception:
            self._subscriber_greenlet = None
            raise

    def destroy(self):
        if self._has_destroy:
            return
        self._has_destroy = True

        self._mngr.del_stream(self.stream_name)
        self._subscriber_greenlet = None
        try:
            self._destroy_internal()
        except Exception:
            pass

    def get_stream_metadata(self, timeout):
        raise StreamSwitchError("Not Implement")

    def get_stream_statistic(self, timeout):
        raise StreamSwitchError("Not Implement")

    def get_client_list(self):
        raise StreamSwitchError("Not Implement")

    def send_rpc_reqeust(self, request, blob, timout):
        pass

    def _start_internal(self):
        pass

    def _destroy_internal(self):
        pass

    def _subscriber_run(self):

        current = gevent.getcurrent()
        subscriber_socket = None

        while (not self._has_destroy) and (current == self._subscriber_greenlet):
            try:
                if subscriber_socket is None:
                    subscriber_socket = self._create_subsriber_socket()

                event = subscriber_socket.poll(0.1)   # wait for 0.1
                if event == zmq.POLLIN:
                    bytes_list = subscriber_socket.recv_multipart(zmq.NOBLOCK)
                    try:
                        channel, packet, blob = self._parse_sub_bytes(bytes_list)
                        self._handle_sub_packet(channel, packet, blob)
                    except Exception:
                        # ignore handler exception
                        pass

            except Exception:
                if subscriber_socket is not None:
                    subscriber_socket.close(0)
                    subscriber_socket = None
                gevent.sleep(1)

        if subscriber_socket is not None:
            subscriber_socket.close(0)
        del subscriber_socket

    def _create_subsriber_socket(self):
        subscriber_endpoint = "ipc://@" + STSW_SOCKET_NAME_STREAM_PREFIX + \
            "/" + self.stream_name + "/" + STSW_SOCKET_NAME_STREAM_PUBLISH
        subscriber_socket = self._mngr.zmq_ctx.socket(zmq.SUB)
        subscriber_socket.setsockopt(zmq.LINGER, 0)
        subscriber_socket.set_string(zmq.SUBSCRIBE, "info")   # only recv the stream info
        subscriber_socket.connect(subscriber_endpoint)
        return subscriber_socket

    def _parse_sub_bytes(self, bytes_list):
        channel = bytes_list[0].decode()
        packet = bytes_list[0] # more parse for packet_bytes
        if len(bytes_list) > 2:
            blob = bytes_list[2]

        return channel, packet, blob

    def _handle_sub_packet(self, channel, packet, blob):

        if self._event_listener is not None and callable(self._event_listener):
            self._event_listener(StreamSubsriberEvent("Stream Subsriber event", self, channel, packet, blob))

    def _handle_stream_info(self, packet):
        pass


class StreamManager(object):
    def __init__(self):
        self.zmq_ctx = zmq.Context.instance()
        self._source_type_map = {}
        self._stream_map = {}
        self._tmp_creating_stream = set()

    def register_source_type(self, source_type, stream_factory):
        self._source_type_map[source_type] = stream_factory

    def unregister_source_type(self, source_type):
        if source_type not in self._source_type_map:
            raise StreamSwitchError("source_type(%s) Not Found" % source_type, 404)
        del self._source_type_map[source_type]

    def new_stream(self, source_type, stream_name, url, api_tcp_port=0, log_file=None, log_size=DEFAULT_LOG_SIZE,
                     log_rotate=DEFAULT_LOG_ROTATE, err_restart_interval=30, extra_args={}, **kargs):
        # check params
        if source_type is None or stream_name is None:
            raise StreamSwitchError("Param error" % source_type, 404)

        stream_factory = self._source_type_map.get(source_type)
        if stream_factory is None:
            raise StreamSwitchError("source_type(%s) Not Register" % source_type, 404)
        if stream_name in self._stream_map:
            raise StreamSwitchError("Stream(%s) already exists" % stream_name, 400)
        if stream_name in self._tmp_creating_stream:
            raise StreamSwitchError("Stream(%s) Creating Conflict" % stream_name, 400)

        self._tmp_creating_stream.add(stream_name)

        # the third-party factory would block
        try:
            stream = stream_factory(self, stream_name, url, api_tcp_port, log_file, log_size,
                                    log_rotate, err_restart_interval, extra_args, **kargs)
            stream.start()
        except Exception:
            self._tmp_creating_stream.discard(stream_name)
            raise

        self._stream_map[stream_name] = stream
        self._tmp_creating_stream.discard(stream_name)

    def list_streams(self):
        return self._stream_map.values()

    def get_stream(self, stream_name):
        stream = self._stream_map.get(stream_name)
        if stream is None:
            raise StreamSwitchError("Stream(%s) Not Found" % stream_name, 404)
        return stream

    def del_stream(self, stream_name):
        if stream_name in self._stream_map:
            self._stream_map.pop(stream_name)

StreamManager = StreamManager()


def test_main():
    pass


if __name__ == "__main__":
    test_main()