"""
streamswitch.wsgiapp.domain.port_info
~~~~~~~~~~~~~~~~~~~~~~~

This module implements the PortInfo class whose instance
includes all attributes of a port and running status

:copyright: (c) 2015 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""
from __future__ import unicode_literals, division

from streamswitch.stream_mngr import DEFAULT_LOG_SIZE
from streamswitch.stream_mngr import DEFAULT_LOG_ROTATE


class PortInfo(object):
    def __init__(self, port, auto_start=False):
        self.__dict__.update(port.__dict__)
        self.is_running = port.is_running()
        self.auto_start = auto_start




