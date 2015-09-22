"""
streamswitch.wsgiapp.daos.port_dao
~~~~~~~~~~~~~~~~~~~~~~~

This module implements the DAO for port configuration's persistence

:copyright: (c) 2015 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""

from ..utils.config import Config
from gevent.threadpool import ThreadPool
from ..utils.schema import Schema, Optional, IntVal, BoolVal, StrRe, \
    Use, DoNotCare, STRING, AutoDel
from ...port_mngr import TRANSPORT_TCP, TRANSPORT_UDP
from ...exceptions import StreamSwitchError
import os
import copy

conf_file_schema = Schema([{
    Optional("listen_port"): IntVal(0, 65535),
    Optional("transport"): IntVal(values=(TRANSPORT_TCP, TRANSPORT_UDP)),
    Optional("ipv6"): BoolVal(),
    Optional("log_file"): StrRe(r"^\S+$"),   # not empty
    Optional("log_size"): IntVal(1024, 100*1024*1024),  # 1K ~ 100M
    Optional("log_rotate"): IntVal(0, 10),   # rotate from 0 to 10
    Optional("err_restart_interval"): Use(float),
    Optional("extra_options"): Schema({DoNotCare(Use(STRING)): Use(STRING)}),
    Optional("desc"): Use(STRING),
    Optional("auto_start"): BoolVal(),
    "port_name": StrRe(r"^\S+$"),
    Optional("port_type"): StrRe(r"^\S+$"),
    "port_factory": StrRe(r"^\S+$"),
    DoNotCare(Use(STRING)): object  # for all other key we don't care
}])


new_port_config_schema = Schema({
    Optional("listen_port"): IntVal(0, 65535),
    Optional("transport"): IntVal(values=(TRANSPORT_TCP, TRANSPORT_UDP)),
    Optional("ipv6"): BoolVal(),
    Optional("log_file"): StrRe(r"^\S+$"),   # not empty
    Optional("log_size"): IntVal(1024, 100*1024*1024),  # 1K ~ 100M
    Optional("log_rotate"): IntVal(0, 10),   # rotate from 0 to 10
    Optional("err_restart_interval"): Use(float),
    Optional("extra_options"): Schema({DoNotCare(Use(STRING)): Use(STRING)}),
    Optional("desc"): Use(STRING),
    Optional("auto_start"): BoolVal(),
    AutoDel("port_name"): object,
    AutoDel("port_type"): object,
    AutoDel("port_factory"): object,
    DoNotCare(Use(STRING)): object  # for all other key we don't care
})


class PortDao(object):
    def __init__(self, config_file):
        self.conf_file = STRING(config_file)
        self._threadpool = ThreadPool(1)
        self._cached_port_conf_list = []
        self._load_conf()

    def _load_conf(self):
        if os.path.isfile(self.conf_file):
            config = self._threadpool.apply(
                Config.from_file,
                (self.conf_file, conf_file_schema))
            self._cached_port_conf_list = config.conf

    def get_port_conf_list(self):
        return copy.deepcopy(self._cached_port_conf_list)

    def get_port_conf(self, port_name):
        for port_config in self._cached_port_conf_list:
            if port_config.get("port_name") == port_name:
                break
        else:
            raise StreamSwitchError("Port (%s) Not Exist in config file(%)" %
                                    (port_name, self.self_conf_file),  404)
        return copy.deepcopy(port_config)

    def update_port_conf(self, port_name, new_port_config):

        new_port_config = new_port_config_schema.validate(new_port_config)

        for port_config in self._cached_port_conf_list:
            if port_config.get("port_name") == port_name:
                break
        else:
            raise StreamSwitchError("Port (%s) Not Exist in config file(%)" %
                                    (port_name, self.self_conf_file),  404)
        port_config.update(new_port_config)

        save_port_conf_list = copy.deepcopy(self._cached_port_conf_list)
        self._threadpool.apply(Config.to_file,
                               (self.conf_file, save_port_conf_list))