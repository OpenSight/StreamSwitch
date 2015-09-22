"""
streamswitch.wsgiapp.utils.logger
~~~~~~~~~~~~~~~~

This module implements the base log utils for streamswitch controller wsgi app.

:copyright: (c) 2014 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""
import logging


logger = logging.getLogger("streamswitch")

LOG_TYPE_CONFIG = "Config"
LOG_TYPE_ERROR = "Error"
LOG_TYPE_GENERAL = "General"


def setLevel(lvl):
    """set the storlever logger's level

    arg:
    lvl -- the level number

    """
    logger.setLevel(lvl)


def log(lvl, log_type, msg, *args, **kwargs):
    """emit a log record to storlever's logger

    arg:
    lvl -- the log's level's number
    log_type -- the log's type, can only be LOG_TYPE_*
    msg -- log's content
    args -- the args list for msg

    """
    log_type_str = "[%s] " % log_type
    logger.log(lvl, log_type_str + msg, *args, **kwargs)

