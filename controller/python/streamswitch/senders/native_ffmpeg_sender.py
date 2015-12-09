"""
streamswitch.sources.native_ffmpeg_sender
~~~~~~~~~~~~~~~~~~~~~~~

This module implements the sender factory of the native ffmpeg sender

:copyright: (c) 2015 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""

from __future__ import unicode_literals, division
from ..sender_mngr import ProcessSender
from ..exceptions import StreamSwitchError

NATIVE_FFMPEG_PROGRAM_NAME = "ffmpeg"
NATIVE_FFMPEG_SENDER_TYPE_NAME = "native_ffmpeg"

class NativeFFmpegSender(ProcessSender):
    _executable = NATIVE_FFMPEG_PROGRAM_NAME

    def __init__(self, stream_name="", **kwargs):
        if stream_name == "":
            raise StreamSwitchError("stream_name cannot be empty for native_ffmpeg", 400)
        super(NativeFFmpegSender, self).__init__(stream_name=stream_name,
                                                 **kwargs)

    def _generate_cmd_args(self):
        if self._executable is None or len(self._executable) == 0:
            program_name = self.sender_type
        else:
            program_name = self._executable

        cmd_args = [program_name]
        extra_options = dict(self.extra_options)
        port_type = extra_options.pop("port_type", "rtsp")
        if port_type == "rtsp":
            host_addr = "127.0.0.1"
            if self.stream_host != "":
                host_addr = self.stream_host
            rtsp_port = 554
            if self.stream_port != 0:
                rtsp_port = self.stream_port
            input_url = "rtsp://%s:%d/stsw/stream/%s" % (host_addr, rtsp_port, self.stream_name)
            cmd_args.extend(["-rtsp_transport", "tcp"])
        else:
            raise StreamSwitchError(
                "port_type(%s) cannot be supported for native_ffmpeg" % port_type,
                400)

        cmd_args.extend(["-i", input_url])

        for k, v in extra_options.items():
            cmd_args.extend(["-%s" % k, "%s" % v])

        if self.dest_format is not None and self.dest_format != "":
            cmd_args.extend(["-f", self.dest_format])

        cmd_args.append(self.dest_url)

        return cmd_args

