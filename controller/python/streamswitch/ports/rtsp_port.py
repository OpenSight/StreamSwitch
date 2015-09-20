"""
streamswitch.rtsp_port
~~~~~~~~~~~~~~~~~~~~~~~

This module implements the rtsp port class

:copyright: (c) 2015 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""

from __future__ import unicode_literals, division
from ..port_mngr import TRANSPORT_TCP, SubProcessPort
from ..exceptions import StreamSwitchError

RTSP_PORT_PROGRAM_NAME = "stsw_rtsp_port"


class RtspPort(SubProcessPort):
    def __init__(self, port_name, port_type=RTSP_PORT_PROGRAM_NAME,
                 listen_port=554,
                 transport=TRANSPORT_TCP, ipv6=False, **kwargs):
        if transport != TRANSPORT_TCP:
            raise StreamSwitchError("RTSP Port Only Support TCP")
        if ipv6:
            raise StreamSwitchError("RTSP Port Not Support IPv6")
        super(RtspPort, self).__init__(
            port_name=port_name,
            port_type=port_type,
            listen_port=listen_port,
            executable=RTSP_PORT_PROGRAM_NAME,
            transport=transport,
            ipv6=ipv6,
            **kwargs)
    def configure(self, transport=None, ipv6=None, **kargs):
        if transport is not None and \
            transport != TRANSPORT_TCP:
            raise StreamSwitchError("RTSP Port Only Support TCP")
        if ipv6 is not None and ipv6:
            raise StreamSwitchError("RTSP Port Not Support IPv6 by now")

        super(RtspPort, self).configure(
            transport=transport,
            ipv6=ipv6,
            **kwargs)



