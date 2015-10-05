"""
streamswitch.port_mngr
~~~~~~~~~~~~~~~~~~~~~~~

This module implements the port server management

:copyright: (c) 2015 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""

from __future__ import unicode_literals, division
from .exceptions import StreamSwitchError
from .process_mngr import spawn_watcher, PROC_RUNNING
from .events import PortStatusChangeEvent
from .utils import STRING
import gevent



DEFAULT_LOG_SIZE = 1024 * 1024
DEFAULT_LOG_ROTATE = 3

TRANSPORT_TCP = 1
TRANSPORT_UDP = 2


class BasePort(object):

    def __init__(self, port_name, port_type="base", listen_port=0, transport=TRANSPORT_TCP, ipv6=False, log_file=None, log_size=DEFAULT_LOG_SIZE,
                 log_rotate=DEFAULT_LOG_ROTATE, err_restart_interval=30.0, desc="", extra_options={}, event_listener=None, **kargs):
        self.port_name = STRING(port_name)
        self.port_type = STRING(port_type)
        self.listen_port = int(listen_port)
        self.transport = int(transport)
        self.ipv6 = bool(ipv6)
        self.log_file = log_file
        self.log_size = int(log_size)
        self.log_rotate = int(log_rotate)
        self.err_restart_interval = float(err_restart_interval)
        self.extra_options = dict(extra_options)
        self.desc = STRING(desc)
        self._event_listener = event_listener

    def __str__(self):
        return ('Port Server %s (port_type:%s, listen_port:%s, transport:%d, ipv6:%s, '
                'log_file:%s, log_size:%d, log_rotate:%d err_restart_interval:%d, extra_options:%s, '
                'desc:%s)') % \
               (self.port_name, self.port_type, self.listen_port, self.transport, self.ipv6,
                self.log_file, self.log_size, self.log_rotate,
                self.err_restart_interval, self.extra_options, self.desc)

    def start(self):
        pass

    def restart(self):
        self.stop()
        self.start()

    def stop(self):
        pass

    def is_running(self):
        return False

    def reload(self):
        pass



    def configure(self, listen_port=None, transport=None, ipv6=None, log_file=None, log_size=None,
                 log_rotate=None, err_restart_interval=None, extra_options=None, desc=None, **kargs):
        if listen_port is not None:
            self.listen_port = int(listen_port)
        if transport is not None:
            self.transport = int(transport)
        if ipv6 is not None:
            self.ipv6 = bool(ipv6)
        if log_file is not None:
            self.log_file = STRING(log_file)
        if log_size is not None:
            self.log_size = int(log_size)
        if log_rotate is not None:
            self.log_rotate = int(log_rotate)
        if err_restart_interval is not None:
            self.err_restart_interval = float(err_restart_interval)
        if extra_options is not None:
            self.extra_options = dict(extra_options)
        if desc is not None:
            self.desc = STRING(desc)



class SubProcessPort(BasePort):

    def __init__(self, executable=None, **kargs):
        super(SubProcessPort, self).__init__(**kargs)
        self._executable = executable
        self._proc_watcher = None
        self.cmd_args = self._generate_cmd_args()

    def __del__(self):
        self.stop()

    def start(self):
        if self._proc_watcher is not None:
            return    # alread start
        self._proc_watcher = spawn_watcher(self.cmd_args,
                                           error_restart_interval=self.err_restart_interval,
                                           process_status_cb=self._process_status_cb)

    def stop(self):
        if self._proc_watcher is not None:
            proc_watcher = self._proc_watcher
            self._proc_watcher = None
            proc_watcher.stop()
            proc_watcher.destroy()

    def is_running(self):
        if self._proc_watcher is not None:
            if self._proc_watcher.process_status == PROC_RUNNING:
                return True
            else:
                return False
        else:
            return False

    def reload(self):
        if self._proc_watcher is not None:
            self.restart()

    def configure(self, **kargs):
        super(SubProcessPort, self).configure(**kargs)
        self.cmd_args = self._generate_cmd_args()

    def _generate_cmd_args(self):
        if self._executable is None or len(self._executable) == 0:
            program_name = self.port_type
        else:
            program_name = self._executable

        cmd_args = [program_name]

        if self.listen_port != 0:
            cmd_args.extend(["-p", "%d" % self.listen_port])

        if self.log_file is not None and self.log_file != "":
            cmd_args.extend(["-l", STRING(self.log_file)])
            cmd_args.extend(["-L", "%d" % self.log_size])
            cmd_args.extend(["-r", "%d" % self.log_rotate])

        for k, v in self.extra_options.items():
            k = k.replace("_", "-")
            cmd_args.append("--%s=%s" % (k, v))

        return cmd_args

    def _process_status_cb(self, proc_watcher):
        try:
            if self._event_listener is not None and callable(self._event_listener):
                self._event_listener(
                    PortStatusChangeEvent("Port Status Change event", self))
        except Exception:
            pass


# private repo variable used by this module

_port_map = {}


def register_port(port_server):
    if port_server.port_name in _port_map:
        raise StreamSwitchError("Port Already Register", 400)
    _port_map[port_server.port_name] = port_server


def unregister_port(port_name):
    if port_name not in _port_map:
        raise StreamSwitchError("Port(%s) Not Found" % port_name, 404)
    del _port_map[port_name]


def list_ports():
    return list(_port_map.values())


def find_port(port_name):
    return _port_map.get(port_name)


def _test_sink_port():
    port = SubProcessPort(port_name="test_port", port_type="stsw_rtsp_port")
    register_port(port)
    assert(len(list_ports()) == 1)
    test_port = find_port("test_port")
    print("before start")
    print(test_port)
    test_port.start()
    print("after start")
    print(test_port)
    assert(test_port.is_running())
    gevent.sleep(3)
    test_port.restart()

    print("after restart")
    print(test_port)
    gevent.sleep(3)
    test_port.stop()
    assert(not test_port.is_running())
    unregister_port("test_port")
    assert(len(list_ports()) == 0)

