"""
streamswitch.utils.process_mngr
~~~~~~~~~~~~~~~~~~~~~~~

This module implements the process watchers and some related
helper functions

:copyright: (c) 2015 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""

from __future__ import unicode_literals, division
import gevent
from gevent import subprocess, sleep

import time

PROC_STOP = 0
PROC_RUNNING = 1

POLL_INTERVAL_SEC = 0.1

_watchers = {}
_next_wid = 0


def _get_next_watcher_id():
    global _next_wid
    _next_wid += 1
    return _next_wid


class ProcWatcher(object):
    def __init__(self, args, restart_interval, listener):
        self.wid = _get_next_watcher_id()
        self.args = args
        self.last_proc_ret = 0
        self.last_proc_exit_time = 0
        self._proc_start_time = 0
        self.restart_count = 0
        self._restart_interval = restart_interval
        self._popen = None
        self._started = False
        self._listener = listener
        self._poll_greenlet_id = 1
        self._watcher_start_time = 0

        # add to the manager
        global _watchers
        if self.wid in _watchers:
            raise KeyError("wid already exist")
        _watchers[self.wid] = self

    def __str__(self):
        return 'ProcWatcher (wid:%s)' % self.wid

    def _launch_process(self):
        self._popen = subprocess.Popen(self.args,
                                       stdin=subprocess.DEVNULL,
                                       stdout=subprocess.DEVNULL,
                                       stderr=subprocess.DEVNULL,
                                       close_fds=True)
        self._proc_start_time = time.time()
        self._on_listener_start()

    def _on_process_terminate(self, ret):
        self._popen = None
        self.last_proc_ret = ret
        self.last_proc_exit_time = time.time()
        self._proc_start_time = 0
        self._on_listener_stop()

    def _on_listener_start(self):
        try:
            if self._listener is not None \
               and hasattr(self._listener, "on_process_start"):
                self._listener.on_process_start(self)
        except Exception:
            pass

    def _on_listener_stop(self):
        try:
            if self._listener is not None \
               and hasattr(self._listener, "on_process_stop"):
                self._listener.on_process_stop(self)
        except Exception:
            pass

    def _polling_run(self, greenlet_id):
        next_open_time = time.time()

        while (greenlet_id == self._poll_greenlet_id) and self._started:
            try:
                if self._popen is None:
                    if (time.time() >= next_open_time) and \
                            self._restart_interval > 0:
                        # restart
                        self.restart_count += 1
                        self._launch_process()
                else:
                    # check the child process
                    ret = self._popen.poll()
                    if ret is not None:
                        # the process terminate
                        self._on_process_terminate(ret)

                        if ret != 0:
                            # exit with error
                            next_open_time = time.time() + self.restart_process()
                        else:
                            # exit normally
                            next_open_time = time.time()

            except Exception:
                pass

            sleep(POLL_INTERVAL_SEC)      # next time to check

    @property
    def pid(self):
        if self._popen is not None:
            return self._popen.pid
        else:
            return None

    @property
    def process_status(self):
        if self._popen is not None:
            return PROC_RUNNING
        else:
            return PROC_STOP

    @property
    def process_running_time(self):
        if self._popen is not None:
            return time.time() - self._proc_start_time
        else:
            return 0

    def is_started(self):
        return self._started

    def start(self):
        if self._started:
            return

        try:

            self._launch_process()   # start up the process

            # spawn a poll greenlet to watch it
            self._started = True
            self._poll_greenlet_id += 1
            gevent.spawn(self._polling_run, self._poll_greenlet_id)

            self._watcher_start_time = 0
            self.restart_count = 0

        except Exception:
            self._started = False
            if self._popen is not None:
                self._popen.kill()
                self._on_process_terminate(-1)
            raise

    def stop(self):
        if not self._started:
            return

        self._started = False   # make poll greenlet exit

        if self._popen is not None:
            # terminate the process normally at first
            self._popen.terminate()
            try:
                ret = self._popen.wait(1)
            except subprocess.TimeoutExpired:
                ret = None

            #check terminated successfully, if not, kill it
            if ret is None:
                # time out, force child process terminate
                self._popen.kill()
                ret = -1

            self._on_process_terminate(ret)

    def delete(self):
        self.stop()

        global _watchers
        if self.wid in _watchers:
            del _watchers[self.wid]


def spawn_watcher(args, restart_interval=30, listener=None):
    """ create asd start a process watcher instance

    Args:
        args: the process command args. args should be a sequence of program
            arguments or else a single string. By default, the program to
            execute is the first item in args if args is a sequence.
            If args is a string, the interpretation is platform-dependent.
            Unless otherwise stated, it is recommended to pass args as a
            sequence.
        restart_interval: time in sec to restart the process after its error termination.
            if the process exit with 0 exit_code, it would be restart immediately.
            if this parameter is 0, means never restart the process

    Returns:
        A started Watcher instance related to the args which is already schedule

    Raises:
        OSError: when trying to execute a non-existent file
    """
    global _next_wid
    _next_wid += 1
    watcher = ProcWatcher(_next_wid, args, restart_interval, listener)
    watcher.start()
    return watcher


def list_all_waitcher():
    return _watchers.values()


def find_watcher_by_wid(wid):
    return _watchers.get(wid)


def test_main():
    # ls_watcher = spawn_watcher(["ls"])
    pass

if __name__ == "__main__":
    test_main()