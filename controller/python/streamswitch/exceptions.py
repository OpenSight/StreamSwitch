"""
streamswitch.exceptions
~~~~~~~~~~~~~~~~~~~~~~~

This module defines the base exception class which should be
sub-classed from by all other Exception used in StreamSwitch Controller project.

:copyright: (c) 2014 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""


class StreamSwitchError(Exception):
    def __init__(self, info, http_status_code=500):
        super(StreamSwitchError, self).__init__(info)
        self.http_status_code = http_status_code


class StreamSwitchCmdError(StreamSwitchError):
    def __init__(self, return_code, info, http_status_code=500):
        super(StreamSwitchCmdError, self).__init__(info, http_status_code)
        self.return_code = return_code


class ExecutableNotFoundError(StreamSwitchError):
    def __init__(self, program_name, http_status_code=404):
        super(ExecutableNotFoundError, self).__init__("%s Not Found" % program_name, http_status_code)
        self.program_name = program_name