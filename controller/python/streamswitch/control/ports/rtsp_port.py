"""
streamswitch.control.sources.proxy_source
~~~~~~~~~~~~~~~~~~~~~~~

This module implements the stream factory of the proxy source type

:copyright: (c) 2015 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""

from __future__ import unicode_literals, division
from ..port_mngr import TRANSPORT_TCP, SubProcessPort
from ...utils.exceptions import StreamSwitchError

RTSP_PORT_PROGRAM_NAME = "stsw_rtsp_port"


class RtspPort(SubProcessPort):
    def __init__(self, transport=TRANSPORT_TCP, ipv6=False, **kwargs):
        if transport != TRANSPORT_TCP:
            raise StreamSwitchError("RTSP Port Only Support TCP")
        if ipv6:
            raise StreamSwitchError("RTSP Port Not Support IPv6")
        super(RtspPort, self).__init__(executable=RTSP_PORT_PROGRAM_NAME,
                                       transport=transport,
                                       ipv6=ipv6,
                                       **kwargs)





