from __future__ import unicode_literals, division, print_function
import sys

import argparse
from streamswitch.exceptions import StreamSwitchError
from streamswitch.process_mngr import spawn_watcher
from gevent import sleep
import signal
import gevent.signal


class WatchCommand(object):

    description = """
This command is a tool to run a program in a process watcher of StreamSwitch,
which provides extra functions like restarting on error, process aging, and etc.
    """

    def __init__(self):

        self.parser = argparse.ArgumentParser(description=self.description)
                                              # formatter_class=argparse.RawDescriptionHelpFormatter)
        self.parser.add_argument("-r", "--restart-interval", type=float, default=30.0,
                                 help="the time(in sec) to restart the process on error (default: %(default)s)")
        self.parser.add_argument("-a", "--age-time", type=float, default=0.0,
                                 help="the running time (in sec) for which this process should be consider to be aged, "
                                      "which result to restart(By TERM signal). "
                                      "Default is %(default)s, meaning disable this function")

        self.parser.add_argument("prog",
                                 help="the program with its arguments to run")
        self.parser.add_argument('args', nargs=argparse.REMAINDER,
                                 help="the arguments for the running program")

        self.args = self.parser.parse_args()
        self.process_cb_listener = self.process_status_cb

    def process_status_cb(self, watcher):
        print("prog %s status changed:" % self.args.prog, watcher)

    def term_handler(self, signum, frame):
        print("Receive sig handler %d, Exit" % signum)
        raise SystemExit

    def run(self):
        # print(self.args)
        gevent.signal.signal(signal.SIGTERM, self.term_handler)
        gevent.signal.signal(signal.SIGINT, self.term_handler)
        watcher = spawn_watcher([self.args.prog] + self.args.args,
                      error_restart_interval=self.args.restart_interval,
                      age_time=self.args.age_time,
                      process_status_cb=self.process_cb_listener)

        gevent.wait()
        watcher.stop()
        watcher.destroy()



def main(argv=sys.argv):
    command = WatchCommand()
    return command.run()


if __name__ == '__main__': # pragma: no cover
    sys.exit(main() or 0)