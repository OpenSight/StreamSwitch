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
    "port_type": StrRe(r"^\S+$"),
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
    def __init__(self, config_file=None):
        self.conf_file = config_file
        self.threadpool = ThreadPool(1)

    def _get_port_conf_list(self):
        config = Config.from_file(self.conf_file, conf_file_schema)
        return config.conf

    def get_port_conf_list(self):
        return self.threadpool.apply(self._get_port_conf_list)

    def _get_port_conf(self, port_name):
        config = Config.from_file(self.conf_file, conf_file_schema)
        for port_config in config.conf:
            if port_config.get("port_name") == port_name:
                break
        else:
            raise StreamSwitchError("Port (%s) Not Exist in config file(%)" %
                                    (port_name, self.self_conf_file),  404)
        return port_config

    def get_port_conf(self, port_name):
        return self.threadpool.apply(self._get_port_conf, port_name)

    def _update_port_conf(self, port_name, new_port_config):
        config = Config.from_file(self.conf_file, conf_file_schema)
        for port_config in config.conf:
            if port_config.get("port_name") == port_name:
                break
        else:
            raise StreamSwitchError("Port (%s) Not Exist in config file(%)" %
                                    (port_name, self.self_conf_file),  404)

        new_port_config = new_port_config_schema.validate(new_port_config)
        port_config.update(new_port_config)
        config.write()

    def update_port_conf(self, port_name, new_port_config):
        return self.threadpool.apply(self._update_port_conf,
                                     port_name,
                                     new_port_config)