"""
streamswitch.wsgiapp.services.port_service
~~~~~~~~~~~~~~~~~~~~~~~

This module implements the service class for port management

:copyright: (c) 2015 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""
from __future__ import unicode_literals, division
from ...exceptions import StreamSwitchError
import gevent
from ..domains.port_info import PortInfo
from ...utils import import_method


class PortService(object):

    def __init__(self, port_mngr=None, port_dao=None):
        self.port_mngr = port_mngr
        self.port_dao = port_dao

    def on_application_created(self, event):
        self.load()

    def load(self):
        port_conf_list = self.port_dao.get_port_conf_list()
        for port_conf in port_conf_list:
            constructor_params = dict(port_conf)
            port_factory = import_method(constructor_params.pop("port_factory"))
            auto_start = constructor_params.pop("auto_start", False)
            port = port_factory(**constructor_params)
            if auto_start:
                port.start()
            self.port_mngr.register_port(port)

    def get_port_list(self):
        port_list = self.port_mngr.list_ports()
        port_conf_list = self.port_dao.get_port_conf_list()

        port_conf_dict = {}
        for port_conf in port_conf_list:
            port_name = port_conf["port_name"]
            port_conf_dict[port_name] = port_conf

        port_info_list =[]
        for port in port_list:
            port_conf = port_conf_dict[port.port_name]
            port_list.append(
                PortInfo(port=port,
                         auto_start=port_conf.get("auto_start", False)))
        return port_info_list

    def get_port(self, port_name):
        port = self.port_mngr.find_port(port_name)
        if port is None:
            raise StreamSwitchError(
                "Port (%s) Not Found" % port_name, 404)
        port_conf = self.port_dao.get_port_conf(port_name)
        return PortInfo(port=port,
                         auto_start=port_conf.get("auto_start", False))

    def stop_port(self, port_name):
        port = self.port_mngr.find_port(port_name)
        if port is None:
            raise StreamSwitchError(
                "Port (%s) Not Found" % port_name, 404)
        port.stop()

    def start_port(self, port_name):
        port = self.port_mngr.find_port(port_name)
        if port is None:
            raise StreamSwitchError(
                "Port (%s) Not Found" % port_name, 404)
        port.start()

    def restart_port(self, port_name):
        port = self.port_mngr.find_port(port_name)
        if port is None:
            raise StreamSwitchError(
                "Port (%s) Not Found" % port_name, 404)
        port.restart()

    def reload_port(self, port_name):
        port = self.port_mngr.find_port(port_name)
        if port is None:
            raise StreamSwitchError(
                "Port (%s) Not Found" % port_name, 404)
        port.reload()

    def configure_port(self, port_name, port_configs):
        port = self.port_mngr.find_port(port_name)
        if port is None:
            raise StreamSwitchError(
                "Port (%s) Not Found" % port_name, 404)

        configure_params = dict(port_configs)
        configure_params.pop("auto_start", None)
        port.configure(**configure_params)

        self.port_dao.update_port_conf(port_name, port_configs)



