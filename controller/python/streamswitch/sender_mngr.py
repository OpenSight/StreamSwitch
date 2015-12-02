"""
streamswitch.sender_mngr
~~~~~~~~~~~~~~~~~~~~~~~

This module implements the stream sender management

:copyright: (c) 2015 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""

from __future__ import unicode_literals, division
from .exceptions import StreamSwitchError
from .events import SenderStateChangeEvent
from .process_mngr import spawn_watcher, PROC_STOP, kill_all, PROC_RUNNING
from .utils import import_method, is_str, STRING


import zmq.green as zmq
import gevent
from gevent import sleep
import time
import traceback
import weakref
import base64
import sys

SENDER_STATE_STR = ["OK", "ERROR", "RESTARTING"]
SENDER_STATE_OK = 0
SENDER_STATE_ERR = 1
SENDER_STATE_RESTARTING = 2

DEFAULT_LOG_SIZE = 1024 * 1024
DEFAULT_LOG_ROTATE = 3
LOG_LEVEL_INFO = 6


class BaseSender(object):

    def __init__(self, sender_type, sender_name, dest_url, dest_format="", stream_name="", stream_host="", stream_port=0,
                  log_file=None, log_size=DEFAULT_LOG_SIZE, log_rotate=DEFAULT_LOG_ROTATE, log_level=LOG_LEVEL_INFO,
                  err_restart_interval=30.0, age_time=0.0, extra_options={}, event_listener=None,
                  **kwargs):
        # check the params
        if len(sender_name) == 0:
            raise StreamSwitchError("Sender name cannot be empty", 400)
        if len(dest_url) == 0:
            raise StreamSwitchError("Sender dest url cannot be empty", 400)
        if len(stream_name) == 0:
            if len(stream_host) == 0:
               raise StreamSwitchError("stream name or host cannot be both empty", 400)


        self.__sender_name = STRING(sender_name)  # stream_name cannot be modified
        if event_listener is not None:
            self._event_listener_weakref = weakref.ref(event_listener)
        else:
            self._event_listener_weakref = None
        self._has_destroy = False
        self._has_started = False


        # config
        self.sender_name = STRING(sender_name)
        self.sender_type = STRING(sender_type)
        self.dest_url = STRING(dest_url)
        self.dest_format = STRING(dest_format)
        self.stream_name = STRING(stream_name)
        self.stream_host = STRING(stream_host)
        self.stream_port = int(stream_port)
        if log_file is not None:
            self.log_file = STRING(log_file)
        else:
            self.log_file = None
        self.log_size = int(log_size)
        self.log_rotate = int(log_rotate)
        self.log_level = int(log_level)
        self.err_restart_interval = float(err_restart_interval)
        self.extra_options = dict(extra_options)
        self.age_time = float(age_time)

        # status
        self.state = SENDER_STATE_OK
        self.state_str = SENDER_STATE_STR[SENDER_STATE_OK]

        global _sender_map
        if sender_name in _sender_map:
            raise StreamSwitchError("Sender(%s) already exists" % sender_name, 400)
        _sender_map[self.__sender_name] = self


    def __str__(self):
        return ('Sender %s (sender_type:%s, dest_url:%s, dest_format:%s'
                 'stream_name:%s, stream_host:%s, stream_port:%d, log_file:%s, '
                 'err_restart_interval:%f, age_time:%f, '
                 'state:%s)') % \
               (self.sender_name, self.sender_type, self.dest_url, self.dest_format,
                self.stream_name, self.stream_host, self.stream_port, self.log_file,
                self.err_restart_interval, self.age_time,
                self.state_str)

    def start(self):
        """ start the stream instance

        This method would be invoked automatically by stream manager when create a new stream

        """
        if self._has_destroy:
            raise StreamSwitchError("Sender Already Destroy", 503)
        if self._has_started:
            return

        self._has_started = True

        try:
            self._start_internal()
        except Exception:
            self._has_started = False
            raise


    def restart(self):
        """ restart the stream

        This method would restart this stream, the underlying
        implementation would reconnect the stream source, and retrieve the stream data
        with a different ssrc

        """
        if self._has_destroy:
            raise StreamSwitchError("Sender Already Destroy", 503)
        if not self._has_started:
            raise StreamSwitchError("Sender Not Start", 503)

        self._restart_internal()

    def destroy(self):
        if self._has_destroy:
            return
        self._has_destroy = True
        # disable the event listener
        self._event_listener_weakref = None
        try:
            self._destroy_internal()
        except Exception:
            pass

    def _set_state(self, new_state):
        old_state = self.state
        self.state = new_state
        self.state_str = SENDER_STATE_STR[new_state]
        if old_state != new_state:
            self._send_event(SenderStateChangeEvent(
                "The state of Sender %s is changed from %s to %s" % (self.sender_name,
                                                                     SENDER_STATE_STR[old_state],
                                                                     self.state_str) ,
                self))

    def _start_internal(self):
        pass

    def _restart_internal(self):
        pass

    def _destroy_internal(self):
        pass


    def _send_event(self, event):
        # print("_send_event")
        if self._event_listener_weakref is None:
            # print("_event_listener_weakref is None")
            return
        event_listener = self._event_listener_weakref()
        if event_listener is not None and callable(event_listener):
            # print("_send_event:call event_listener")
            try:
                event_listener(event)
            except Exception:
                pass # ignore all exception throw by user's listener


class ProcessSender(BaseSender):
    _executable = None

    def __init__(self, **kwargs):
        super(SourceProcessStream, self).__init__(**kwargs)
        self._proc_watcher = None
        self.cmd_args = self._generate_cmd_args()
        self._process_status_cb_ref = None

    def _generate_cmd_args(self):
        if self._executable is None or len(self._executable) == 0:
            program_name = self.source_type
        else:
            program_name = self._executable

        cmd_args = [program_name, "-u", self.dest_url]

        if self.dest_format is not None and self.dest_format != "":
            cmd_args.extend(["-f", self.dest_format])

        # source
        if self.stream_name != "":
            cmd_args.extend(["-s", self.stream_name])
        if self.stream_host != "":
            cmd_args.extend(["-H", self.stream_host])
            cmd_args.extend(["-p", "%d" % self.stream_port])

        # log
        cmd_args.extend(["--log-level", "%d" % self.log_level])
        if self.log_file is not None and self.log_file != "":
            cmd_args.extend(["-l", self.log_file])
            cmd_args.extend(["-L", "%d" % self.log_size])
            cmd_args.extend(["-r", "%d" % self.log_rotate])


        for k, v in self.extra_options.items():
            k = k.replace("_", "-")
            cmd_args.append("--%s=%s" % (k, v))

        return cmd_args

    def _start_internal(self):
        if self._proc_watcher is not None:
            self._proc_watcher.destroy()
        self._process_status_cb_ref = self._process_status_cb
        self._proc_watcher = spawn_watcher(self.cmd_args,
                                           error_restart_interval=self.err_restart_interval,
                                           age_time=self.age_time,
                                           process_status_cb=self._process_status_cb_ref)

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
        self._process_status_cb_ref = None

    def _process_status_cb(self, proc_watcher):
        if self._proc_watcher is proc_watcher:
            if proc_watcher.process_status == PROC_START:
                self._set_state(SENDER_STATE_OK)
            elif proc_watcher.process_status == PROC_STOP:
                if proc_watcher.process_return_code == 0:
                    self._set_state(SENDER_STATE_RESTARTING)
                else:
                    self._set_state(SENDER_STATE_ERR)


# private repo variable used by this module
_sender_type_map = {}
_sender_map = weakref.WeakValueDictionary()


def register_sender_type(sender_type, sender_factory):
    if sender_type is None or len(sender_type) == 0:
        raise StreamSwitchError("sender_type invalid", 400)
    if is_str(sender_factory):
        try:
            sender_factory = import_method(sender_factory)
        except Exception:
            raise StreamSwitchError("sender_factory invalid", 400)
    elif not callable(sender_factory):
        raise StreamSwitchError("sender_factory invalid", 400)
    _sender_type_map[sender_type] = sender_factory


def unregister_sender_type(sender_type):
    if sender_type not in _sender_type_map:
        raise StreamSwitchError("sender_type(%s) Not Found" % sender_type, 404)
    del _sender_type_map[sender_type]


def list_sender_types():
    return list(_sender_type_map.keys())


def create_sender(sender_type, sender_name, dest_url, dest_format="", stream_name="", stream_host="", stream_port=0,
                     log_file=None, log_size=DEFAULT_LOG_SIZE, log_rotate=DEFAULT_LOG_ROTATE, log_level=LOG_LEVEL_INFO,
                     err_restart_interval=30.0, age_time=0.0, extra_options={}, event_listener=None,
                     **kwargs):
    # check params
    if sender_type is None or sender_name is None:
        raise StreamSwitchError("Param error", 404)

    sender_factory = _sender_type_map.get(sender_type)
    if sender_factory is None:
        raise StreamSwitchError("sender_type(%s) Not Register" % sender_type, 404)

    sender = sender_factory(sender_type=sender_type, sender_name=sender_name,
                            dest_url=dest_url, dest_format=dest_format,
                            stream_name=stream_name, stream_host=stream_host, stream_port=stream_port,
                            log_file=log_file, log_size=log_size,
                            log_rotate=log_rotate, log_level=log_level,
                            err_restart_interval=err_restart_interval,
                            age_time=age_time,
                            extra_options=extra_options,
                            event_listener=event_listener,
                            **kwargs)

    # the third-party factory would block
    try:
        sender.start()
    except Exception:
        sender.destroy()
        raise
    return sender


def list_senders(sender_type=None):
    if sender_type is None:
        return list(_sender_map.values())
    sender_list = []
    for sender in _sender_map.values():
        if sender.sender_type == sender_type:
            sender_list.append(sender)
    return sender_list


def find_sender(sender_name):
    return _sender_map.get(sender_name)


def _test_text_sink_stream():

    # clear up
    kill_all("text_sink")

    #register_source_type("file_live_source", SourceProcessStream)
    test_sender = create_sender(sender_type="text_sink",
                                sender_name="test_sender",
                                dest_url="/dev/null",
                                dest_format="mpegts",
                                stream_name="abc")
    print("new test_sender:")
    print(test_sender)
    print("cmd_args: %s" % test_sender.cmd_args)
    assert(test_sender.state == SENDER_STATE_OK)

    gevent.sleep(2)
    print("after 2 sec, test_sender:")
    print(test_sender)
    assert(test_sender.state == STREAM_STATE_OK)


    print("restart test_sender")
    test_sender.restart()
    assert(test_sender.state == STREAM_STATE_OK)
    print(test_sender)
    gevent.sleep(0.1)
    test_stream.destroy()

    pass


def _test_base_stream():

    register_sender_type("base_sender", BaseSender)
    print("Sender type list:")
    print(list_sender_types())
    # assert(list_source_types() == ["base_stream"])
    test_sender = create_sender(sender_type="base_sender",
                                sender_name="test_sender",
                                dest_url="/test_sender",
                                stream_name="test_stream")
    assert(test_sender == find_stream("test_sender"))
    assert(len(list_senders()) == 1)
    print("new test_sender:")
    print(test_sender)
    assert(test_sender.sender_name == "test_sender")
    assert(test_sender.state == SENDER_STATE_OK)
    gevent.sleep(1)
    print("after 1 sec, test_sender:")
    print(test_sender)
    assert(test_sender.state == SENDER_STATE_OK)

    test_sender.destroy()
    del test_sender
    gevent.sleep(1)
    assert(len(list_senders()) == 0)
    unregister_sender_type("base_sender")
    # assert(len(list_source_types()) == 0)


