
from gevent import Greelet, subprocess, sleep

import time

PROC_STOP = 0
PROC_RUNNING = 1
PROC_ERROR = 2


_watchers = {}
_next_wid = 0

class ProcWathcer(Greenlet):
    def __init__(self, wid, args, restart_interval):
        Greenlet.__init__(self)
        self.wid = wid
        self.args = args
        self.process_status = PROC_STOP
        self.last_ret = 0
        self._restart_interval = restart_interval
        self._popen = None
        self._started = False

        # add to the manager
        global _watchers
        if wid in _watchers:
            raise KeyError("wid already exist")
        _watchers[wid] = self

    def __str__(self):
        return 'ProcWatcher (%s)' % self.seconds

    def child_create(self):
        self._popen = subprocess.Popen(self.args,
                                       stdin=subprocess.DEVNULL,
                                       stdout=subprocess.DEVNULL,
                                       stderr=subprocess.DEVNULL,
                                       close_fds=True)
        self.process_status = PROC_RUNNING



    def _run(self):
        next_open_time = time.time()

        while self.started:
            if self._popen is None:
                if (time.time() >= next_open_time) and \
                        self._restart_interval > 0:
                    # restart

                    pass
                else:
                    sleep(0.1)
            else:
                # check the child process
                try:
                    ret = self._popen.wait(0.1)
                except subprocess.TimeoutExpired:
                    ret = None

                if ret is not None:
                    # the process terminate
                    self.last_ret = ret
                    self._popen = None

                    if ret != 0:
                        # exit with error
                        self.process_status = PROC_ERROR
                        next_open_time = time.time() + self.restart_process()
                    else:
                        # exit normally
                        self.process_status = PROC_STOP
                        next_open_time = time.time()

        # Check child process again at last
        if self._popen is not None:

            try:
                ret = self._popen.wait(1)
            except subprocess.TimeoutExpired:
                ret = None

            if ret is not None:
                # child process terminated
                self.last_ret = ret
                self._popen = None
                if ret != 0:
                    # exit with error
                    self.process_status = PROC_ERROR
                else:
                    # exit normally
                    self.process_status = PROC_STOP
            else:
                # time out, force child process terminate
                self._popen.kill()
                self.last_ret = 0
                self._popen = None
                self.process_status = PROC_ERROR



    def start(self):
        Greenlet.start(self)
        self.started = True

    def restart_process(self):
        pass

    def delete(self):
        if self.started:
            #kill the process
            self.started = False

        global _watchers
        if self.wid in _watchers:
            del _watchers[self.wid]






def spawn_watcher(args, restart_interval=30):
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

    """
    global _next_wid
    _next_wid += 1
    watcher = ProcWathcer(_next_wid, args, restart_interval)
    watcher.start()
    return watcher





def list_all_waitcher():
    return _watchers.values()



def test_main():
    pass


if __name__ == "__main__":
    test_main()