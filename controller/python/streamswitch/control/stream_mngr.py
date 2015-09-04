"""
streamswitch.control.stream_mngr
~~~~~~~~~~~~~~~~~~~~~~~

This module implements the input stream management

:copyright: (c) 2015 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""

from __future__ import unicode_literals, division
from ..utils.exceptions import StreamSwitchError
from ..utils.events import StreamSubsriberEvent, StreamInfoEvent
from ..utils.process_mngr import spawn_watcher, PROC_STOP, kill_all
from ..utils.utils import import_method, is_str
from ..pb import pb_packet_pb2
from ..pb import pb_stream_info_pb2
from ..pb import pb_metadata_pb2
from ..pb import pb_client_list_pb2
from ..pb import pb_media_statistic_pb2
from ..pb import pb_client_heartbeat_pb2

import zmq.green as zmq
import gevent
from gevent import sleep
import time

DEFAULT_LOG_SIZE = 1024 * 1024
DEFAULT_LOG_ROTATE = 3

STREAM_STATE_CONNECTING = pb_stream_info_pb2.PROTO_SOURCE_STREAM_STATE_CONNECTING
STREAM_STATE_OK = pb_stream_info_pb2.PROTO_SOURCE_STREAM_STATE_OK
# error
STREAM_STATE_ERR = pb_stream_info_pb2.PROTO_SOURCE_STREAM_STATE_ERR
STREAM_STATE_ERR_CONNECT_FAIL = pb_stream_info_pb2.PROTO_SOURCE_STREAM_STATE_ERR_CONNECT_FAIL
STREAM_STATE_ERR_MEIDA_STOP = pb_stream_info_pb2.PROTO_SOURCE_STREAM_STATE_ERR_MEIDA_STOP
STREAM_STATE_ERR_TIME = pb_stream_info_pb2.PROTO_SOURCE_STREAM_STATE_ERR_TIME
STREAM_STATE_ERR_TIMEOUT = pb_stream_info_pb2.PROTO_SOURCE_STREAM_STATE_ERR_TIMEOUT

PLAY_TYPE_LIVE = pb_metadata_pb2.PROTO_PLAY_TYPE_LIVE
PLAY_TYPE_REPLAY = pb_metadata_pb2.PROTO_PLAY_TYPE_REPLAY

STREAM_MODE_ACTIVE = "active"
STREAM_MODE_PASSIVE = "passive"

SUB_STREAM_MEIDA_TYPE_VIDEO = pb_metadata_pb2.PROTO_SUB_STREAM_MEIDA_TYPE_VIDEO
SUB_STREAM_MEIDA_TYPE_AUDIO = pb_metadata_pb2.PROTO_SUB_STREAM_MEIDA_TYPE_AUDIO
SUB_STREAM_MEIDA_TYPE_TEXT = pb_metadata_pb2.PROTO_SUB_STREAM_MEIDA_TYPE_TEXT
SUB_STREAM_MEIDA_TYPE_PRIVATE = pb_metadata_pb2.PROTO_SUB_STREAM_MEIDA_TYPE_PRIVATE

SUB_STREAM_DIRECTION_OUTBOUND = pb_metadata_pb2.PROTO_SUB_STREAM_DIRECTION_OUTBOUND
SUB_STREAM_DIRECTION_INBOUND = pb_metadata_pb2.PROTO_SUB_STREAM_DIRECTION_INBOUND

IP_VERSION_V4 = pb_client_heartbeat_pb2.PROTO_IP_VERSION_V4
IP_VERSION_V6 = pb_client_heartbeat_pb2.PROTO_IP_VERSION_V6

STSW_SOCKET_NAME_STREAM_PREFIX = "stsw_stream"
STSW_SOCKET_NAME_STREAM_PUBLISH = "broadcast"
STSW_SOCKET_NAME_STREAM_API = "api"

STSW_PUBLISH_INFO_CHANNEL = "info"
STSW_PUBLISH_MEDIA_CHANNEL = "media"

STREAM_DEBUG_FLAG_DUMP_API = 1     # dump the api request/reply
STREAM_DEBUG_FLAG_DUMP_PUBLISH = 2     # dump  publish/subsribe message

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


class BaseStream(object):

    DEBUG_FLAGS = 0    # no debug
    API_SEQ_MASK = 0xffffffff
    STSW_STREAM_STATE_TIMEOUT_TIME = 300
    SUBSCRIBER_QUEUE_SIZE = 16

    def __init__(self, source_type, stream_name, url, api_tcp_port=0, log_file=None, log_size=DEFAULT_LOG_SIZE,
                 log_rotate=DEFAULT_LOG_ROTATE, err_restart_interval=30.0, extra_options={}, event_listener=None,
                 **kwargs):
        self.__stream_name = stream_name  # stream_name cannot be modified
        self._event_listener = event_listener
        self._has_destroy = False
        self._has_started = False
        self._api_client_socket = None
        self._subscriber_greenlet = None
        self._api_seq = 1

        # config
        self.stream_name = stream_name
        self.source_type = source_type
        self.url = url
        self.api_tcp_port = api_tcp_port
        self.log_file = log_file
        self.log_size = log_size
        self.log_rotate = log_rotate
        self.err_restart_interval = err_restart_interval
        self.extra_options = extra_options
        self.mode = STREAM_MODE_ACTIVE

        # status
        self.state = STREAM_STATE_CONNECTING
        self.play_type = PLAY_TYPE_LIVE
        self.source_protocol = ""
        self.ssrc = 0
        self.cur_bps = 0
        self.last_frame_time = 0.0
        self.update_time = time.time()
        self.client_num = 0

        # add to the global manager
        if stream_name in _started_stream_map:
            raise StreamSwitchError("Stream(%s) already exists" % stream_name, 400)
        if stream_name in _tmp_creating_streams:
            raise StreamSwitchError("Stream(%s) Creating Conflict" % stream_name, 400)
        _tmp_creating_streams.add(stream_name)


    def __str__(self):
        return ('Stream %s (source_type:%s, url:%s, api_tcp_port:%d, log_file:%s,'
                'log_size:%d, log_rotate:%d err_restart_interval:%d, mode:%s, '
                'state:%d, play_type:%d, source_protocol:%s, ssrc:%d, cur_bps:%d, '
                'last_frame_time:%f, update_time:%f, client_num:%d)') % \
               (self.stream_name, self.source_type, self.url, self.api_tcp_port,
                self.log_file, self.log_size, self.log_rotate,
                self.err_restart_interval, self.mode, self.state, self.play_type,
                self.source_protocol, self.ssrc, self.cur_bps,
                self.last_frame_time, self.update_time, self.client_num)

    def start(self):
        """ start the stream instance

        This method would be invoked automatically by stream manager when create a new stream

        """
        if self._has_destroy:
            raise StreamSwitchError("Stream Already Destroy", 503)
        if self._has_started:
            return

        self._subscriber_greenlet = gevent.spawn(self._subscriber_run)
        try:
            self._start_internal()
        except Exception:
            self._subscriber_greenlet = None
            raise

        self._has_started = True
        _started_stream_map[self.__stream_name] = self
        _tmp_creating_streams.discard(self.__stream_name)

    def restart(self):
        """ restart the stream

        This method would restart this stream, the underlying
        implementation would reconnect the stream source, and retrieve the stream data
        with a different ssrc

        """
        if self._has_destroy:
            raise StreamSwitchError("Stream Already Destroy", 503)
        if not self._has_started:
            raise StreamSwitchError("Stream Not Start", 503)

        self._restart_internal()
        if self.state >= 0:
            self.state = STREAM_STATE_CONNECTING
            self.update_time = time.time()

    def destroy(self):
        if self._has_destroy:
            return
        self._has_destroy = True

        if self.__stream_name in _started_stream_map:
            del _started_stream_map[self.__stream_name]
        _tmp_creating_streams.discard(self.__stream_name)

        self._subscriber_greenlet = None

        if self._api_client_socket is not None:
            api_socket = self._api_client_socket
            self._api_client_socket = None
            api_socket.close(0)
        try:
            self._destroy_internal()
        except Exception:
            pass

    def get_stream_metadata(self, timeout=5.0):
        if self._has_destroy:
            raise StreamSwitchError("stream already destroy", 503)
        request = self._new_request(pb_packet_pb2.PROTO_PACKET_CODE_METADATA)

        if (self.DEBUG_FLAGS & STREAM_DEBUG_FLAG_DUMP_API) != 0:
            print("Encode no body into a PROTO_PACKET_CODE_METADATA request")

        reply, rep_blob = self.send_rpc_request(request, None, timeout)
        metadata_rep = pb_metadata_pb2.ProtoMetaRep.FromString(reply.body)

        if (self.DEBUG_FLAGS & STREAM_DEBUG_FLAG_DUMP_API) != 0:
            print("Decode the following body from a PROTO_PACKET_CODE_METADATA reply:")
            print(metadata_rep)

        # construct the value object
        metadata = StreamMetaData()
        metadata.play_type = metadata_rep.play_type
        metadata.source_protocol = metadata_rep.source_proto
        metadata.stream_len = metadata_rep.stream_len
        metadata.ssrc = metadata_rep.ssrc
        metadata.bps = metadata_rep.bps

        for sub_stream in metadata_rep.sub_streams:
            sub_stream_metadata = SubStreamMetaData()
            sub_stream_metadata.index = sub_stream.index
            sub_stream_metadata.media_type = sub_stream.media_type
            sub_stream_metadata.codec_name = sub_stream.codec_name
            sub_stream_metadata.direction = sub_stream.direction
            sub_stream_metadata.extra_data = sub_stream.extra_data
            sub_stream_metadata.height = sub_stream.height
            sub_stream_metadata.width = sub_stream.width
            sub_stream_metadata.fps = sub_stream.fps
            sub_stream_metadata.fov = sub_stream.gov
            sub_stream_metadata.samples_per_second = sub_stream.samples_per_second
            sub_stream_metadata.channels = sub_stream.channels
            sub_stream_metadata.bits_per_sample = sub_stream.bits_per_sample
            sub_stream_metadata.samples_per_frame = sub_stream.sampele_per_frame
            sub_stream_metadata.x = sub_stream.x
            sub_stream_metadata.y = sub_stream.y
            sub_stream_metadata.font_size = sub_stream.fone_size
            sub_stream_metadata.font_type = sub_stream.font_type
            metadata.sub_streams.append(sub_stream_metadata)

        return metadata

    def key_frame(self, timeout=5.0):
        if self._has_destroy:
            raise StreamSwitchError("stream already destroy", 503)
        request = self._new_request(pb_packet_pb2.PROTO_PACKET_CODE_KEY_FRAME)

        if (self.DEBUG_FLAGS & STREAM_DEBUG_FLAG_DUMP_API) != 0:
            print("Encode no body into a PROTO_PACKET_CODE_KEY_FRAME request")

        self.send_rpc_request(request, None, timeout)

        if (self.DEBUG_FLAGS & STREAM_DEBUG_FLAG_DUMP_API) != 0:
            print("Decode no body from a PROTO_PACKET_CODE_KEY_FRAME reply")

    def get_stream_statistic(self, timeout=5.0):
        if self._has_destroy:
            raise StreamSwitchError("stream already destroy", 503)
        request = self._new_request(pb_packet_pb2.PROTO_PACKET_CODE_MEDIA_STATISTIC)

        if (self.DEBUG_FLAGS & STREAM_DEBUG_FLAG_DUMP_API) != 0:
            print("Encode no body into a PROTO_PACKET_CODE_MEDIA_STATISTIC request")

        reply, rep_blob = self.send_rpc_request(request, None, timeout)
        statistic_rep = pb_media_statistic_pb2.ProtoMediaStatisticRep.FromString(reply.body)

        if (self.DEBUG_FLAGS & STREAM_DEBUG_FLAG_DUMP_API) != 0:
            print("Decode the following body from a PROTO_PACKET_CODE_MEDIA_STATISTIC reply:")
            print(statistic_rep)

        # construct the value object
        stream_statistic = StreamStatistic()
        stream_statistic.ssrc = statistic_rep.ssrc
        stream_statistic.sum_bytes = statistic_rep.sum_bytes
        stream_statistic.timestamp = float(statistic_rep.timestamp) / 1000.0
        for sub_stream_stat in statistic_rep.sub_stream_stats:
            sub_stream_statistic = SubStreamStatistic()
            sub_stream_statistic.sub_stream_index = sub_stream_stat.sub_stream_index
            sub_stream_statistic.media_type = sub_stream_stat.media_type
            sub_stream_statistic.data_bytes = sub_stream_stat.data_bytes
            sub_stream_statistic.key_bytes = sub_stream_stat.key_bytes
            sub_stream_statistic.lost_frames = sub_stream_stat.lost_frames
            sub_stream_statistic.data_frames = sub_stream_stat.data_frames
            sub_stream_statistic.key_frames = sub_stream_stat.key_bytes
            sub_stream_statistic.last_gov = sub_stream_stat.last_gov
            stream_statistic.sub_stream_stats.append(sub_stream_statistic)
        return stream_statistic

    def get_client_list(self, start_index, max_client_num, timeout=5.0):
        if self._has_destroy:
            raise StreamSwitchError("stream already destroy", 503)

        client_list_request_body = pb_client_list_pb2.ProtoClientListReq()
        client_list_request_body.start_index = start_index
        client_list_request_body.client_num = max_client_num
        request = self._new_request(pb_packet_pb2.PROTO_PACKET_CODE_CLIENT_LIST)
        request.body = client_list_request_body.SerializeToString()

        if (self.DEBUG_FLAGS & STREAM_DEBUG_FLAG_DUMP_API) != 0:
            print("Encode the following body into a PROTO_PACKET_CODE_CLIENT_LIST request:")
            print(client_list_request_body)

        reply, rep_blob = self.send_rpc_request(request, None, timeout)
        client_list_rep = pb_client_list_pb2.ProtoClientListRep.FromString(reply.body)

        if (self.DEBUG_FLAGS & STREAM_DEBUG_FLAG_DUMP_API) != 0:
            print("Decode the following body from a PROTO_PACKET_CODE_CLIENT_LIST reply::")
            print(client_list_rep)

        # construct the value object
        client_list = ClientList()
        client_list.total_num = client_list_rep.total_num
        client_list.start_index = client_list_rep.start_index
        for client in client_list_rep.client_list:
            client_info = ClientInfo()
            client_info.client_ip_version = client.client_ip_version
            client_info.client_ip = client.client_ip
            client_info.client_port = client.client_port
            client_info.client_token = client.client_token
            client_info.client_protocol = client.client_protocol
            client_info.client_text = client.client_text
            client_info.last_active_time = float(client.last_active_time)
            client_list.client_list.append(client_info)

        return client_list

    def send_rpc_request(self, request_packet, blob = None, timeout = 5.0):
        if request_packet is None:
            raise StreamSwitchError("request_packet cannot be None", 400)
        timeout = float(timeout)
        if timeout <= 0.0:
            raise StreamSwitchError("timeout invalid", 400)
        if request_packet.header.type != pb_packet_pb2.PROTO_PACKET_TYPE_REQUEST:
            raise StreamSwitchError("packet type invalid", 400)
        if self._has_destroy:
            raise StreamSwitchError("stream already destroy", 503)
        if not self._has_started:
            raise StreamSwitchError("stream not start", 500)

        api_socket = self._api_client_socket
        self._api_client_socket = None

        if api_socket is None:
            api_socket = self._create_api_socket()

        # send request
        try:
            send_bytes_list = [request_packet.SerializeToString()]
            if blob is not None:
                send_bytes_list.append(blob)

            api_socket.send_multipart(send_bytes_list)

            event = api_socket.poll(int(timeout * 1000))   # wait for timeout
            if event != zmq.POLLIN:
                raise StreamSwitchError("Requet Timeout", 503)
            recv_bytes_list = api_socket.recv_multipart(zmq.NOBLOCK)

        except Exception:
            if api_socket is not None:
                api_socket.close(0)
                raise

        if (not self._has_destroy) and (self._api_client_socket is None):
            self._api_client_socket = api_socket
        else:
            if api_socket is not None:
                api_socket.close(0)


        # parse the reply
        rep_packet, blob = self._parse_rep_bytes(recv_bytes_list)
        if rep_packet.header.type != pb_packet_pb2.PROTO_PACKET_TYPE_REPLY or \
           rep_packet.header.seq != request_packet.header.seq or \
           rep_packet.header.code != request_packet.header.code:
            raise StreamSwitchError("Reply invalid", 500)

        if rep_packet.header.status < 200 or \
            rep_packet.header.status >= 300:
            raise StreamSwitchError(rep_packet.header.info, rep_packet.header.status)

        return rep_packet, blob

    def _start_internal(self):
        pass

    def _restart_internal(self):
        pass

    def _destroy_internal(self):
        pass

    def _subscriber_run(self):

        current = gevent.getcurrent()
        subscriber_socket = None

        while (not self._has_destroy) and (current == self._subscriber_greenlet):
            try:
                # print("subscriber_run is running")
                if subscriber_socket is None:
                    subscriber_socket = self._create_subsriber_socket()

                event = subscriber_socket.poll(0.1)   # wait for 0.1
                if event == zmq.POLLIN:
                    bytes_list = subscriber_socket.recv_multipart(zmq.NOBLOCK)
                    try:
                        channel, packet, blob = self._parse_sub_bytes(bytes_list)
                        self._handle_sub_packet(channel, packet, blob)
                        if self._event_listener is not None and callable(self._event_listener):
                            self._event_listener(
                                StreamSubsriberEvent("Stream Subsriber event",
                                                     self, channel, packet, blob))
                    except Exception:
                        # ignore handler exception
                        pass
            except Exception:
                if subscriber_socket is not None:
                    subscriber_socket.close(0)
                    subscriber_socket = None
                gevent.sleep(1)

            # timer task
            now = time.time()
            self._check_stream_state(now)

        if subscriber_socket is not None:
            subscriber_socket.close(0)

    def _check_stream_state(self, now):
        if self.state >= 0:
            if now - self.update_time >= self.STSW_STREAM_STATE_TIMEOUT_TIME:
                self.state = STREAM_STATE_ERR_TIMEOUT

    def _create_subsriber_socket(self):
        subscriber_endpoint = "ipc://@" + STSW_SOCKET_NAME_STREAM_PREFIX + \
            "/" + self.stream_name + "/" + STSW_SOCKET_NAME_STREAM_PUBLISH
        subscriber_socket = _zmq_ctx.socket(zmq.SUB)
        subscriber_socket.setsockopt(zmq.LINGER, 0)
        subscriber_socket.setsockopt(zmq.RCVHWM, self.SUBSCRIBER_QUEUE_SIZE)
        subscriber_socket.setsockopt(zmq.SNDHWM, self.SUBSCRIBER_QUEUE_SIZE)
        subscriber_socket.set_string(zmq.SUBSCRIBE, STSW_PUBLISH_INFO_CHANNEL)   # only recv the stream info
        subscriber_socket.connect(subscriber_endpoint)
        return subscriber_socket

    def _create_api_socket(self):
        api_endpoint = "ipc://@" + STSW_SOCKET_NAME_STREAM_PREFIX + \
            "/" + self.stream_name + "/" + STSW_SOCKET_NAME_STREAM_API
        api_socket = _zmq_ctx.socket(zmq.REQ)
        api_socket.setsockopt(zmq.LINGER, 0)
        api_socket.connect(api_endpoint)
        return api_socket

    def _parse_sub_bytes(self, bytes_list):
        channel = bytes_list[0].decode()
        packet = pb_packet_pb2.ProtoCommonPacket.FromString(bytes_list[1])   # parse for packet_bytes
        blob = None
        if len(bytes_list) > 2:
            blob = bytes_list[2]
        return channel, packet, blob

    def _parse_rep_bytes(self, bytes_list):
        packet = pb_packet_pb2.ProtoCommonPacket.FromString(bytes_list[0])   # parse for packet_bytes
        blob = None
        if len(bytes_list) > 1:
            blob = bytes_list[1]
        return packet, blob

    def _new_request(self, code):
        request = pb_packet_pb2.ProtoCommonPacket()
        request.header.type = pb_packet_pb2.PROTO_PACKET_TYPE_REQUEST
        request.header.seq = self._get_next_api_seq()
        request.header.code = code
        return request

    def _handle_sub_packet(self, channel, packet, blob):
        if channel == STSW_PUBLISH_INFO_CHANNEL:
            if packet.header.code == pb_packet_pb2.PROTO_PACKET_CODE_STREAM_INFO:
                self._handle_stream_info(packet, blob)

    def _handle_stream_info(self, packet, blob=None):
        stream_info = pb_stream_info_pb2.ProtoStreamInfoMsg.FromString(packet.body)
        send_time = float(stream_info.send_time) / 1000.0
        if (time.time() - send_time >=
                self.STSW_STREAM_STATE_TIMEOUT_TIME):
            return    # ignore the too old info

        if stream_info.ssrc == self.ssrc and \
           send_time < self.update_time:
            return  # ignore the out-time stream info

        self.state = stream_info.state
        self.play_type = stream_info.play_type
        self.source_protocol = stream_info.source_proto
        self.ssrc = stream_info.ssrc
        self.cur_bps = stream_info.cur_bps
        self.last_frame_time = \
            float(stream_info.last_frame_sec) + \
            float(stream_info.last_frame_usec) / 1000000.0
        self.update_time = send_time
        self.client_num = stream_info.client_num

        if self._event_listener is not None and callable(self._event_listener):
            self._event_listener(
                StreamInfoEvent("Stream info event", self, stream_info))

    def _get_next_api_seq(self):
        seq = self._api_seq & self.API_SEQ_MASK
        self._api_seq += 1
        return seq


class SourceProcessStream(BaseStream):
    _executable = None

    def __init__(self, **kwargs):
        super(SourceProcessStream, self).__init__(**kwargs)
        self._proc_watcher = None
        self.mode = STREAM_MODE_ACTIVE
        self.cmd_args = self._generate_cmd_args()

    def _generate_cmd_args(self):
        if self._executable is None or len(self._executable) == 0:
            program_name = self.source_type
        else:
            program_name = self._executable

        cmd_args = [program_name, "-s", self.stream_name]

        cmd_args.extend(["-p", "%d" % self.api_tcp_port])

        if self.log_file is not None:
            cmd_args.extend(["-l", self.log_file])
            cmd_args.extend(["-L", "%d" % self.log_size])
            cmd_args.extend(["-r", "%d" % self.log_rotate])

        cmd_args.extend(["-u", self.url])

        for k, v in self.extra_options.items():
            cmd_args.append("--%s=%s" % (k, v))

        return cmd_args

    def _start_internal(self):
        if self._proc_watcher is not None:
            self._proc_watcher.destroy()
        self._proc_watcher = spawn_watcher(self.cmd_args,
                                           error_restart_interval=self.err_restart_interval,
                                           process_status_cb=self._process_status_cb)

    def _restart_internal(self):
        if self._proc_watcher is not None:
            self._proc_watcher.stop() # may sleep

        if self._proc_watcher is not None:
            self._proc_watcher.start()

    def _destroy_internal(self):
        if self._proc_watcher is not None:
            self._proc_watcher.stop()
            self._proc_watcher.destroy()
            self._proc_watcher = None

    def _process_status_cb(self, proc_watcher):
        if self._proc_watcher is proc_watcher and \
                proc_watcher.process_status == PROC_STOP:
            if (proc_watcher.process_return_code != 0) and (self.state >= 0):
                self.state = STREAM_STATE_ERR


# private repo variable used by this module
_source_type_map = {}
_started_stream_map = {}
_tmp_creating_streams = set()
_zmq_ctx = zmq.Context.instance()


def register_source_type(source_type, stream_factory):
    if source_type is None or len(source_type) == 0:
        raise StreamSwitchError("source_type invalid", 400)
    if is_str(stream_factory):
        try:
            stream_factory = import_method(stream_factory)
        except Exception:
            raise StreamSwitchError("stream_factory invalid", 400)
    elif not callable(stream_factory):
        raise StreamSwitchError("stream_factory invalid", 400)
    _source_type_map[source_type] = stream_factory


def unregister_source_type(source_type):
    if source_type not in _source_type_map:
        raise StreamSwitchError("source_type(%s) Not Found" % source_type, 404)
    del _source_type_map[source_type]


def list_source_types():
    return list(_source_type_map.keys())


def create_stream(source_type, stream_name, url, api_tcp_port=0, log_file=None, log_size=DEFAULT_LOG_SIZE,
                     log_rotate=DEFAULT_LOG_ROTATE, err_restart_interval=30.0,
                     extra_options={}, event_listener=None, **kwargs):
    # check params
    if source_type is None or stream_name is None:
        raise StreamSwitchError("Param error" % source_type, 404)

    stream_factory = _source_type_map.get(source_type)
    if stream_factory is None:
        raise StreamSwitchError("source_type(%s) Not Register" % source_type, 404)

    stream = stream_factory(source_type=source_type, stream_name=stream_name,
                            url=url, api_tcp_port=api_tcp_port,
                            log_file=log_file, log_size=log_size,
                            log_rotate=log_rotate,
                            err_restart_interval=err_restart_interval,
                            extra_options=extra_options,
                            event_listener=event_listener, **kwargs)

    # the third-party factory would block
    try:
        stream.start()
    except Exception:
        stream.destroy()
        raise

    return stream


def list_streams(source_type=None):
    if source_type is None:
        return list(_started_stream_map.values())

    stream_list = []
    for stream_name, stream in _started_stream_map.items():
        if stream.source_type == source_type:
            stream_list.append(stream)
    return stream_list


def find_stream(stream_name):
    return _started_stream_map.get(stream_name)


def _test_source_process_stream():
    # clear up
    kill_all("file_live_source")

    register_source_type("file_live_source", SourceProcessStream)
    test_stream = create_stream(source_type="file_live_source",
                                stream_name="test_file_stream",
                                url="/dev/zero")
    print("new test_file_stream:")
    print(test_stream)
    print("cmd_args: %s" % test_stream.cmd_args)
    assert(test_stream.state == STREAM_STATE_CONNECTING)

    gevent.sleep(2)
    print("after 2 sec, test_file_stream:")
    print(test_stream)
    assert(test_stream.state == STREAM_STATE_OK)

    metadata = test_stream.get_stream_metadata()
    assert(metadata.source_protocol == "FileSystem")
    assert(metadata.play_type == PLAY_TYPE_LIVE)

    test_stream.key_frame()

    statistic = test_stream.get_stream_statistic()
    assert(statistic.ssrc != 0)
    assert(statistic.sum_bytes > 0)
    assert(statistic.timestamp != 0)
    assert(metadata.ssrc == statistic.ssrc)

    print("restart test_file_stream")
    test_stream.restart()
    assert(test_stream.state == STREAM_STATE_CONNECTING)
    print(test_stream)
    gevent.sleep(0.1)
    print("after restart 0.1 sec, test_file_stream:")
    print(test_stream)
    assert(test_stream.state == STREAM_STATE_CONNECTING)
    gevent.sleep(3)
    print("after restart 3 sec, test_file_stream:")
    print(test_stream)
    assert(test_stream.state == STREAM_STATE_OK)
    new_metadata = test_stream.get_stream_metadata()
    assert(metadata.ssrc != new_metadata.ssrc)
    test_stream.destroy()
    unregister_source_type("file_live_source")


def _test_base_stream():
    register_source_type("base_stream", BaseStream)
    print("Source type list:")
    print(list_source_types())
    assert(list_source_types() == ["base_stream"])
    test_stream = create_stream(source_type="base_stream",
                                stream_name="test_stream",
                                url="stsw://123")
    assert(test_stream == find_stream("test_stream"))
    assert(len(list_streams()) == 1)
    print("new test_stream:")
    print(test_stream)
    assert(test_stream.stream_name == "test_stream")
    assert(test_stream.state == STREAM_STATE_CONNECTING)
    gevent.sleep(1)
    print("after 1 sec, test_stream:")
    print(test_stream)
    assert(test_stream.state == STREAM_STATE_CONNECTING)
    BaseStream.STSW_STREAM_STATE_TIMEOUT_TIME = 5   # modify a constant for test
    gevent.sleep(10)
    print("after 10 sec, test_stream:")
    print(test_stream)
    assert(test_stream.state == STREAM_STATE_ERR_TIMEOUT)

    test_stream.destroy()

    assert(len(list_streams()) == 0)

    unregister_source_type("base_stream")
    assert(len(list_source_types()) == 0)


if __name__ == "__main__":
    _test_base_stream()
    _test_source_process_stream()
    pass