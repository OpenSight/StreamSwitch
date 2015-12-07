"""
streamswitch.wsgiapp.services.stream_service
~~~~~~~~~~~~~~~~~~~~~~~

This module implements the service class for stream management

:copyright: (c) 2015 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""
from __future__ import unicode_literals, division
from ...exceptions import StreamSwitchError
from gevent.event import Event
from ...events import StreamInfoEvent
import gevent
from ...process_mngr import kill_all
from ...senders import FFMPEG_SENDER_TYPE, TEXT_SINK_SENDER_TYPE
from ..models import SenderConf




class SenderService(object):

    def __init__(self, sender_mngr=None, sender_conf_dao=None, dao_context_mngr=None):
        self._sender_mngr = sender_mngr
        self._sender_conf_dao = sender_conf_dao
        self._dao_context_mngr = dao_context_mngr
        self._sender_map = {}
        self._sender_list = []

    def on_application_created(self, event):
        # print("stream service ........load")
        kill_all(FFMPEG_SENDER_TYPE)
        kill_all(TEXT_SINK_SENDER_TYPE)
        self.load()

    def load(self):
        if self._sender_conf_dao is None \
            or self._dao_context_mngr is None:
            return

        with self._dao_context_mngr.context():
            sender_conf_list = self._sender_conf_dao.get_all_sender_conf()

        supported_sender_types = self._sender_mngr.list_sender_types()
        for sender_conf in sender_conf_list:
            if sender_conf.sender_type in supported_sender_types:
                sender = self._sender_mngr.create_sender(
                    sender_type=sender_conf.sender_type,
                    sender_name=sender_conf.sender_name,
                    dest_url=sender_conf.dest_url,
                    dest_format=sender_conf.dest_format,
                    stream_name=sender_conf.stream_name,
                    stream_host=sender_conf.stream_host,
                    stream_port=sender_conf.stream_port,
                    log_file=sender_conf.log_file,
                    log_size=sender_conf.log_size,
                    log_rotate=sender_conf.log_rotate,
                    log_level=sender_conf.log_level,
                    err_restart_interval=sender_conf.err_restart_interval,
                    age_time=sender_conf.age_time,
                    extra_options=sender_conf.extra_options,
                    event_listener=None,
                    **sender_conf.other_kwargs)
                self._sender_map[sender_conf.sender_name] = sender
                self._sender_list.append(sender)

    def get_sender_list(self):
        return list(self._sender_list)

    def get_sender(self, sender_name):
        sender = self._sender_map.get(sender_name)
        if sender is None:
            raise StreamSwitchError(
                "sender (%s) Not Found" % sender_name, 404)
        return sender

    def del_sender(self, sender_name):
        with self._dao_context_mngr.context():
            sender = self._sender_map.get(sender_name)
            if sender is None:
                raise StreamSwitchError(
                    "Sender (%s) Not Found" % sender_name, 404)
            self._sender_conf_dao.del_sender_conf(sender_name)
            del self._sender_map[sender_name]
            try:
                self._sender_list.remove(sender)
            except ValueError:
                pass

            sender.destroy()   # may wait



    def new_sender(self, sender_configs):
        with self._dao_context_mngr.context():

            sender_conf = SenderConf(**sender_configs)
            self._sender_conf_dao.add_sender_conf(sender_conf)

            sender = \
                self._sender_mngr.create_sender(**sender_configs)
            self._sender_map[sender_conf.sender_name] = sender
            self._sender_list.append(sender)

        return sender

    def get_sender_type_list(self):
        return self._sender_mngr.list_sender_types()

    def restart_sender(self, sender_name):
        sender = self._sender_map.get(sender_name)
        if sender is None:
            raise \
                StreamSwitchError( "Sender (%s) Not Found" % sender_name, 404)
        sender.restart()
