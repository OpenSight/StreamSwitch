"""
streamswitch.wsgiapp.views.stream_api_views
~~~~~~~~~~~~~~~~~~~~~~~

This module implements the view callabes of restful api for stream management

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
from ..utils import logger
import logging

def includeme(config):
    config.add_route('senders', '/senders')
    config.add_route('sender', '/senders/{sender_name}')
    config.add_route('sender_types', '/sender_types')
    config.add_route('sender_operations', '/senders/{sender_name}/operations')


def get_sender_service_from_request(request):
    registry = request.registry
    return registry.settings['sender_service']


@get_view(route_name='sender_types')
def get_sender_type_list(request):
    sender_service = get_sender_service_from_request(request)
    return sender_service.get_sender_type_list()


@get_view(route_name='senders')
def get_sender_list(request):
    sender_service = get_sender_service_from_request(request)
    return sender_service.get_sender_list()


new_sender_schema = Schema({
    "sender_name": StrRe(r"^\S+$"),   # not empty
    "sender_type": StrRe(r"^\S+$"),   # not empty
    "dest_url": StrRe(r"^\S+$"),   # not empty
    Optional("dest_format"): StrRe(r"^\S*$"),
    Optional("stream_name"): StrRe(r"^\S*$"),
    Optional("stream_host"): StrRe(r"^\S*$"),
    Optional("stream_port"): IntVal(0, 65535),
    Optional("log_file"): StrRe(r"^\S+$"),
    Optional("log_size"): IntVal(1024, 100*1024*1024),  # 1K ~ 100M
    Optional("log_rotate"): IntVal(0, 10),   # rotate from 0 to 10
    Optional("log_level"): IntVal(0, 7),
    Optional("err_restart_interval"): Use(float),
    Optional("age_time"): Use(float),
    Optional("extra_options"): Schema({DoNotCare(Use(STRING)): Use(STRING)}),
    DoNotCare(Use(STRING)): object  # for all other key we don't care
})


@post_view(route_name='senders')
def add_sender(request):
    sender_configs = get_params_from_request(request, new_sender_schema)
    sender_service = get_sender_service_from_request(request)
    sender = sender_service.new_sender(sender_configs)

    # setup reponse
    response = request.response
    response.status_int = 201
    response.headers["Location"] = \
        request.route_url('sender', sender_name=sender.sender_name)
    return sender


@get_view(route_name='sender')
def get_sender(request):
    sender_name = request.matchdict['sender_name']
    sender_service = get_sender_service_from_request(request)
    return sender_service.get_sender(sender_name)


@delete_view(route_name='sender')
def delete_sender(request):
    sender_name = request.matchdict['sender_name']
    sender_service = get_sender_service_from_request(request)
    sender_service.del_sender(sender_name)
    return Response(status=200)



sender_op_schema = Schema({
    "op":  StrRe(r"^restart$"),   # not em
    DoNotCare(Use(STRING)): object  # for all other key we don't care
})
@post_view(route_name='sender_operations')
def post_sender_operations(request):
    sender_name = request.matchdict['sender_name']
    params = get_params_from_request(request, sender_op_schema)
    sender_service = get_sender_service_from_request(request)
    op = params["op"]
    if op == "restart":
        sender_service.restart_sender(sender_name)
    return Response(status=200)