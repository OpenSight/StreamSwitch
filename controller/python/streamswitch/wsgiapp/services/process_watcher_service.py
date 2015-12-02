"""
streamswitch.wsgiapp.services.process_watcher_service
~~~~~~~~~~~~~~~~~~~~~~~

This module implements the service class for process watcher monitor

:copyright: (c) 2015 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""
from __future__ import unicode_literals, division
from ...exceptions import StreamSwitchError
import gevent





class ProcessWatcherService(object):

    def __init__(self, process_mngr=None):
        self._process_mngr = process_mngr

    def get_process_watcher_list(self):
        return self._process_mngr.list_all_waitcher()

    def get_process_watcher(self, wid):
        watcher = self._process_mngr.find_watcher_by_wid(wid)
        if watcher is None:
            raise StreamSwitchError(
                "Process Watcher (wid:%d) Not Found" % wid, 404)
        return watcher
