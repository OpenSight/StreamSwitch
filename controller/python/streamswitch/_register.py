from .sources.proxy_source import PROXY_SOURCE_PROGRAM_NAME, \
    PROXY_SOURCE_TYPE_NAME, ProxySourceStream
from .sources.rtsp_source import RTSP_SOURCE_PROGRAM_NAME, \
    RTSP_SOURCE_TYPE_NAME, RtspSourceStream
from .sources.file_live_source import FILE_LIVE_SOURCE_PROGRAM_NAME, \
    FILE_LIVE_SOURCE_TYPE_NAME, FileLiveSourceStream

from .stream_mngr import register_source_type
from .utils import find_executable


def _register_builtin_source_type():
    if find_executable(PROXY_SOURCE_PROGRAM_NAME):
        register_source_type(PROXY_SOURCE_TYPE_NAME, ProxySourceStream)
    if find_executable(RTSP_SOURCE_PROGRAM_NAME):
        register_source_type(RTSP_SOURCE_TYPE_NAME, RtspSourceStream)
    if find_executable(FILE_LIVE_SOURCE_PROGRAM_NAME):
        register_source_type(FILE_LIVE_SOURCE_TYPE_NAME, FileLiveSourceStream)


_register_builtin_source_type()

