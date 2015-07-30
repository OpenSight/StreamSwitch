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
DEFAULT_STOP_WAIT_TIMEOUT = 3

_watchers = {}
_next_wid = 0


def _get_next_watcher_id():
    global _next_wid
    _next_wid += 1
    return _next_wid


class ProcWatcher(object):
    def __init__(self, args, 
                 error_restart_interval, success_restart_interval, 
                 process_status_cb):
        self.wid = _get_next_watcher_id()
        self.args = args
        self.process_return_code = 0
        self.process_exit_time = 0
        self.restart_count = 0
        self._proc_start_time = 0
        self._error_restart_interval = error_restart_interval
        self._success_restart_interval = success_restart_interval
        self._popen = None
        self._started = False
        self._process_status_cb = process_status_cb
        self._poll_greenlet_id = 1
        self._watcher_start_time = 0

        # add to the manager
        global _watchers
        if self.wid in _watchers:
            raise KeyError("wid already exist")
        _watchers[self.wid] = self

    def __str__(self):
        return ('ProcWatcher (wid:%s, args:%s, pid:%s, process_status:%d,'
                'process_return_code:%d, process_exit_time:%f '
                'process_running_time:%f, restart_count:%d, is_started:%s)') % \
               (self.wid, self.args, self.pid, self.process_status,
                self.process_return_code, self.process_exit_time,
                self.process_running_time, self.restart_count,
                self.is_started())

    def _launch_process(self):

        self._popen = subprocess.Popen(self.args,
                                       stdin=subprocess.DEVNULL,
                                       stdout=subprocess.DEVNULL,
                                       stderr=subprocess.DEVNULL,
                                       close_fds=True,
                                       shell=False)
        # print("lanch new process %s, pid:%d" % (self.args, self._popen.pid))
        self._proc_start_time = time.time()
        self._on_process_status_change()

    def _on_process_terminate(self, ret):
        # print("process pid:%d terminated with returncode:%d" % (self._popen.pid, ret))
        self._popen = None
        self.process_return_code = ret
        self.process_exit_time = time.time()
        self._proc_start_time = 0
        self._on_process_status_change()

    def _on_process_status_change(self):
        try:
            if self._process_status_cb is not None and \
               callable(self._process_status_cb):
                self._process_status_cb(self)
        except Exception:
            pass

    def _polling_run(self, greenlet_id):
        next_open_time = time.time()

        while (greenlet_id == self._poll_greenlet_id) and self._started:
            try:
                if self._popen is None:
                    if time.time() >= next_open_time:
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
                            next_open_time = time.time() + self._error_restart_interval
                        else:
                            # exit normally
                            next_open_time = time.time() + self._success_restart_interval

            except Exception:
                pass

            sleep(POLL_INTERVAL_SEC)      # next time to check

    @staticmethod
    def _terminate_run(popen, wait_timeout):
        
        ret = None
        try:
            ret = popen.wait(wait_timeout)
        except subprocess.TimeoutExpired:
            ret = None
        except Exception:
            pass

        #check terminated successfully, if not, kill it
        if ret is None:
            # time out, force child process terminate
            popen.kill()

    
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
            
    def restart_process(self):
        if self._popen is not None:
            self._popen.terminate()
            
    def is_started(self):
        return self._started

    def start(self):
        """ start the watcher and launch its process
        
        """
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

    def stop(self, wait_timeout=DEFAULT_STOP_WAIT_TIMEOUT):
        """ Stop the watcher and wait until the related process terminates
    
        Stop this watcher, send SIGTERM signal to the process, 
        wait until the process exits or wait_timeout is due, 
        if the process has not yet terminated, kill it. 
    
        After this function return, the process should have been terminated
    
        Args:
            self: watcher instance
            wait_timeout: the time to wait for the process's termination before kill it
        
        """
        if not self._started:
            return

        self._started = False   # make poll greenlet exit

        if self._popen is not None:
            # terminate the process normally at first
            self._popen.terminate()
            ret = None
            try:
                ret = self._popen.wait(wait_timeout)
            except subprocess.TimeoutExpired:
                ret = None
            except Exception:
                pass
            
            #check terminated successfully, if not, kill it
            if ret is None:
                # time out, force child process terminate
                self._popen.kill()
                ret = -1

            self._on_process_terminate(ret)
    
    def async_stop(self, wait_timeout=DEFAULT_STOP_WAIT_TIMEOUT):
        """ Stop the watcher, and terminate the process async
    
        After this function return, the watcher has been stopped, but the process 
        may or may not have been terminated. It postpone the process termination 
        waiting operation in another new greenlet
    
        Args:
            self: watcher instance
            wait_timeout: the time to wait for the process's termination before kill it
        
        """
        if not self._started:
            return

        self._started = False   # make poll greenlet exit

        if self._popen is not None:     
            self._popen.terminate()            
            gevent.spawn(ProcWatcher._terminate_run, self._popen, wait_timeout)
            self._on_process_terminate(0)
    
    def delete(self):
        self.async_stop(DEFAULT_STOP_WAIT_TIMEOUT)

        global _watchers
        if self.wid in _watchers:
            del _watchers[self.wid]


def spawn_watcher(args, 
                  error_restart_interval=30, success_restart_interval=1, 
                  process_status_cb=None):
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
            

    Returns:
        A started Watcher instance related to the args which is already schedule

    Raises:
        OSError: when trying to execute a non-existent file
    """
    watcher = ProcWatcher(args, 
                          error_restart_interval, success_restart_interval, 
                          process_status_cb)
    watcher.start()
    return watcher


def list_all_waitcher():
    return _watchers.values()


def find_watcher_by_wid(wid):
    return _watchers.get(wid)


def test_main():
    print("create ls watcher")
    ls_watcher = spawn_watcher(["ls"])
    print(ls_watcher)
    assert(ls_watcher.pid is not None)
    assert(ls_watcher.process_status == PROC_RUNNING)
    assert(ls_watcher.process_running_time > 0)
    assert(ls_watcher.is_started())
    print("enter sleep")
    sleep(0.5)
    # the process should terminated
    print("after 0.5s:")
    print(ls_watcher)
    assert(ls_watcher.pid is None)
    assert(ls_watcher.process_status == PROC_STOP)
    assert(ls_watcher.process_running_time == 0)
    assert(ls_watcher.process_return_code == 0)
    assert(ls_watcher.process_exit_time != 0)
    assert(ls_watcher.is_started())
    print("enter sleep")
    sleep(1)
    # the process should restart
    print("after 1.5s:")
    print(ls_watcher)
    assert(ls_watcher.restart_count > 0)
    ls_watcher.stop()
    ls_watcher.delete()
    del ls_watcher

    print("create \"sleep\" watcher")
    sleep_watcher = spawn_watcher(["sleep", "5"], 1, 1)
    print(sleep_watcher)
    org_pid = sleep_watcher.pid
    assert(sleep_watcher.pid is not None)
    assert(sleep_watcher.process_status == PROC_RUNNING)
    assert(sleep_watcher.process_running_time > 0)
    assert(sleep_watcher.is_started())
    print("restart process")
    sleep_watcher.restart_process()
    print("enter sleep")
    sleep(0.5)
    # the process should terminated
    print("after 0.5s:")
    print(sleep_watcher)
    assert(sleep_watcher.pid is None)
    assert(sleep_watcher.process_status == PROC_STOP)
    assert(sleep_watcher.process_running_time == 0)
    assert(sleep_watcher.process_exit_time != 0)
    print("enter sleep")
    sleep(1)
    print("after 1.5s:")
    print(sleep_watcher)
    assert(sleep_watcher.restart_count > 0)
    assert(sleep_watcher.pid != org_pid)
    assert(sleep_watcher.pid is not None)
    assert(sleep_watcher.process_status == PROC_RUNNING)
    assert(sleep_watcher.process_running_time > 0)
    assert(sleep_watcher.is_started())
    print("stop \"sleep\" watcher")
    sleep_watcher.stop()
    print(sleep_watcher)
    assert(sleep_watcher.pid is None)
    assert(sleep_watcher.process_status == PROC_STOP)
    assert(sleep_watcher.process_running_time == 0)
    assert(sleep_watcher.process_exit_time != 0)

    print("start \"sleep\" watcher")
    sleep_watcher.start()
    print(sleep_watcher)
    assert(sleep_watcher.pid is not None)
    assert(sleep_watcher.process_status == PROC_RUNNING)
    assert(sleep_watcher.process_running_time > 0)
    assert(sleep_watcher.is_started())
    print("enter sleep")
    sleep(1)

    sleep_watcher.delete()

if __name__ == "__main__":
    test_main()