"""
streamswitch.control.port_mngr
~~~~~~~~~~~~~~~~~~~~~~~

This module implements the port server management

:copyright: (c) 2015 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""

from __future__ import unicode_literals, division
from ..utils.exceptions import StreamSwitchError
from ..utils.events import StreamSubsriberEvent, StreamInfoEvent
from ..utils.process_mngr import spawn_watcher, PROC_STOP, kill_all


import zmq.green as zmq
import gevent
from gevent import sleep
import time

DEFAULT_LOG_SIZE = 1024 * 1024
DEFAULT_LOG_ROTATE = 3

TRANSPORT_TCP = 1
TRANSPORT_UDP= 2


class BasePortServer(object):

    DEBUG_FLAGS = 0    # no debug

    def __init__(self, port_name, port_type, listen_port, transport=TRANSPORT_TCP, ipv6=False, log_file=None, log_size=DEFAULT_LOG_SIZE,
                 log_rotate=DEFAULT_LOG_ROTATE, err_restart_interval=30.0, extra_options={}, event_listener=None, **kargs):
        self.port_name = port_name
        self.port_type = port_type
        self.listen_port = int(listen_port)
        self.transport = transport
        self.ipv6 = ipv6
        self.log_file = log_file
        self.log_size = log_size
        self.log_rotate = log_rotate
        self.err_restart_interval = err_restart_interval
        self.extra_options = extra_options

        self.event_listener = event_listener

    def __del__(self):
        self.stop()

    def __str__(self):
        return ('Port Server %s (port_type:%s, listen_port:%s, transport:%d, ipv6:%s,'
                'log_file:%s, log_size:%d, log_rotate:%d err_restart_interval:%d, extra_options:%s') % \
               (self.port_name, self.port_type, self.listen_port, self.api_tcp_port,
                self.log_file, self.log_size, self.log_rotate,
                self.err_restart_interval, self.mode, self.state, self.play_type,
                self.source_protocol, self.ssrc, self.cur_bps,
                self.last_frame_time, self.update_time, self.client_num)

    def start(self):
        pass

    def restart(self):
        self.stop()
        self.start()

    def stop(self):
        pass

    def status(self):
        return False

    def reload(self):
        self.restart(self)



class ProcessPortServer(BasePortServer):
    executable_name = None

    def __init__(self, *args, **kargs):
        super(ProcessPortServer, self).__init__(*args, **kargs)
        self.proc_watcher = None


    @property
    def cmd_args(self):
        return self._generate_cmd_args()

    def _generate_cmd_args(self):
        if self.executable_name is None or len(self.executable_name) == 0:
            program_name = self.source_type
        else:
            program_name = self.executable_name

        cmd_args = [program_name, "-s", self.stream_name]

        cmd_args.extend(["-p", "%d" % self.api_tcp_port])

        if self.log_file is not None:
            cmd_args.extend(["-l", self.log_file])
            cmd_args.extend(["-L", "%d" % self.log_size])
            cmd_args.extend(["-r", "%d" % self.log_rotate])

        cmd_args.extend(["-u", self.url])

        for k, v in self.extra_conf.items():
            cmd_args.append("--%s=%s" % (k, v))

        return cmd_args




# private repo variable used by this module
_port_server_map = {}


def register_port_server(port_server):
    if port_server.port_name in _port_server_map:
        raise StreamSwitchError("Port Already Register", 400)
    _port_server_map[port_server.port_name] = port_server


def unregister_port_server(port_name):
    if port_name not in _port_server_map:
        raise StreamSwitchError("Port(%s) Not Found" % port_name, 404)
    del _port_server_map[port_name]


def list_ports():
    return list(_port_server_map.values())


def find_port(port_name):
    return _port_server_map.get(port_name)







if __name__ == "__main__":

    pass