from .sources.proxy_source import PROXY_SOURCE_PROGRAM_NAME, \
    PROXY_SOURCE_TYPE_NAME, ProxySourceStream
from .sources.rtsp_source import RTSP_SOURCE_PROGRAM_NAME, \
    RTSP_SOURCE_TYPE_NAME, RtspSourceStream
from .stream_mngr import register_source_type
from .utils import find_executable


def _register_builtin_source_type():
    if find_executable(PROXY_SOURCE_PROGRAM_NAME) is not None:
        register_source_type(PROXY_SOURCE_TYPE_NAME, ProxySourceStream)
    if find_executable(RTSP_SOURCE_PROGRAM_NAME) is not None:
        register_source_type(RTSP_SOURCE_TYPE_NAME, RtspSourceStream)


_register_builtin_source_type()