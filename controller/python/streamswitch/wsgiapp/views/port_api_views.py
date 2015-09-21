"""
streamswitch.wsgiapp.views.port_api_views
~~~~~~~~~~~~~~~~~~~~~~~

This module implements the view callabes of restful api for port management

:copyright: (c) 2015 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""

from __future__ import unicode_literals, division
from .common import (get_view, post_view,
                                   put_view, delete_view)
from ...exceptions import StreamSwitchError

from pyramid.response import Response
from ..utils.schema import Schema, Optional, DoNotCare, \
    Use, IntVal, Default, SchemaError, BoolVal, StrRe, ListVal, Or, STRING, \
    FloatVal, AutoDel
from .common import get_params_from_request
import gevent
from ...port_mngr import TRANSPORT_TCP, TRANSPORT_UDP


def includeme(config):
    config.add_route('ports', '/ports')
    config.add_route('port', '/ports/{port_name}')
    config.add_route('port_operations', '/ports/{port_name}/operations')


def get_port_service_from_request(request):
    registry = request.registry
    return registry.settings['port_service']


@get_view(route_name='ports')
def get_ports(request):
    port_service = get_port_service_from_request(request)
    return port_service.get_port_list()


@get_view(route_name='port')
def get_port(request):
    port_name = request.matchdict['port_name']
    port_service = get_port_service_from_request(request)
    return port_service.get_port(port_name)


put_port_schema = Schema({
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


@put_view(route_name='port')
def put_port(request):
    port_name = request.matchdict['port_name']
    port_configs = get_params_from_request(request, put_port_schema)
    port_service = get_port_service_from_request(request)
    port_service.configure_port(port_name, port_configs)
    return Response(status=200)


port_op_schema = Schema({
    "op":  StrRe(r"^(start|stop|restart|reload)$"),
    DoNotCare(Use(STRING)): object  # for all other key we don't care
})


@post_view(route_name='port_operations')
def post_port_operations(request):
    port_name = request.matchdict['port_name']
    params = get_params_from_request(request, put_port_schema)
    port_service = get_port_service_from_request(request)
    op = params["op"]

    if op == "start":
        port_service.start_port(port_name)
    elif op == "stop":
        port_service.stop_port(port_name)
    elif op == "reload":
        port_service.reload_port(port_name)
    elif op == "restart":
        port_service.restart_port(port_name)

    return Response(status=200)

