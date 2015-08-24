"""
streamswitch.control.sources.proxy_source
~~~~~~~~~~~~~~~~~~~~~~~

This module implements the stream factory of the proxy source type

:copyright: (c) 2015 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""

from __future__ import unicode_literals, division
from ..stream_mngr import register_source_type, SourceProcessStream
from ...utils.exceptions import ExecutableNotFoundError
from ...utils.utils import find_executable

PROXY_SOURCE_PROGRAM_NAME = "stsw_rtsp_source"


class ProxySourceStream(SourceProcessStream):
    executable_name = PROXY_SOURCE_PROGRAM_NAME


def register_proxy_source_type():
    if find_executable(PROXY_SOURCE_PROGRAM_NAME) is None:
        raise ExecutableNotFoundError(PROXY_SOURCE_PROGRAM_NAME)
    register_source_type("proxy", ProxySourceStream)


