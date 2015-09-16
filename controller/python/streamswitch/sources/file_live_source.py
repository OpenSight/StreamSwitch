"""
streamswitch.sources.rtsp_source
~~~~~~~~~~~~~~~~~~~~~~~

This module implements the stream factory of the default RTSP source type

:copyright: (c) 2015 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""

from __future__ import unicode_literals, division
from ..stream_mngr import SourceProcessStream


FILE_LIVE_SOURCE_PROGRAM_NAME = "file_live_source"
FILE_LIVE_SOURCE_TYPE_NAME = "file_live_source"


class FileLiveSourceStream(SourceProcessStream):
    _executable = FILE_LIVE_SOURCE_PROGRAM_NAME


