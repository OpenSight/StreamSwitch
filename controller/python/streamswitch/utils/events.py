"""
streamswitch.utils.events
~~~~~~~~~~~~~~~~~~~~~~~

This module defines the events class used in StreamSwitch Controller project.
The other event used by other modules should inherit these event

:copyright: (c) 2015 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""


class StreamSwitchEvent(object):
    def __init__(self, info):
        self.info = info


class StreamSubsriberEvent(StreamSwitchEvent):
    """subscriber message event

    When a packet is received from subscriber socket, this event
    would be passed to the event listener after processing
    """
    def __init__(self, info, stream, channel, packet, blob):
        super(StreamSubsriberEvent, self).__init__(info)
        self.stream = stream
        self.channel = channel
        self.packet = packet
        self.blob = blob


class StreamInfoEvent(StreamSwitchEvent):
    """ Stream Info receving event

    When a stream info message is handling by the stream, this
    event would be passed to the event listener after processing

    """
    def __init__(self, info, stream, stream_info):
        super(StreamSubsriberEvent, self).__init__(info)
        self.stream = stream
        self.stream_info = stream_info
