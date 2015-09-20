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
from ..domains.stream_config import StreamConfig


def includeme(config):
    config.add_route('streams', '/streams')
    config.add_route('stream', '/streams/{stream_name}')
    config.add_route('source_types', '/source_types')
    config.add_route('stream_clients', '/streams/{stream_name}/clients')
    config.add_route('stream_metadata', '/streams/{stream_name}/metadata')
    config.add_route('stream_statistic', '/streams/{stream_name}/statistic')
    config.add_route('stream_key_frames', '/streams/{stream_name}/key_frames')
    config.add_route('stream_subsequent_stream_info', '/streams/{stream_name}/subsequent_stream_info')
    config.add_route('stream_operations', '/streams/{stream_name}/operations')


def get_stream_service_from_request(request):
    registry = request.registry
    return registry.settings['stream_service']


@get_view(route_name='source_types')
def get_source_type_list(request):
    stream_service = get_stream_service_from_request(request)
    return stream_service.get_source_type_list()


@get_view(route_name='streams')
def get_stream_list(request):
    stream_service = get_stream_service_from_request(request)
    return stream_service.get_stream_list()


new_stream_schema = Schema({
    "stream_name": StrRe(r"^\S+$"),   # not empty
    "source_type": StrRe(r"^\S+$"),   # not empty
    "url": StrRe(r"^\S+$"),     # not empty
    Optional("api_tcp_port"): IntVal(0, 65535),
    Optional("log_file"): StrRe(r"^\S+$"),   # not empty
    Optional("log_size"): IntVal(1024, 100*1024*1024),  # 1K ~ 100M
    Optional("log_rotate"): IntVal(0, 10),   # rotate from 0 to 10
    Optional("err_restart_interval"): Use(float),
    Optional("extra_options"): Schema({DoNotCare(Use(STRING)): Use(STRING)}),
    DoNotCare(Use(STRING)): object  # for all other key we don't care
})


@post_view(route_name='streams')
def add_stream(request):
    stream_configs = get_params_from_request(request, new_stream_schema)
    stream_service = get_stream_service_from_request(request)
    stream = stream_service.new_stream(stream_configs)

    # setup reponse
    response = request.response
    response.status_int = 201
    response.headers["Location"] = \
        request.route_url('stream', stream_name=stream.stream_name)
    return stream


@get_view(route_name='stream')
def get_stream(request):
    stream_name = request.matchdict['stream_name']
    stream_service = get_stream_service_from_request(request)
    return stream_service.get_stream(stream_name)


@delete_view(route_name='stream')
def delete_stream(request):
    stream_name = request.matchdict['stream_name']
    stream_service = get_stream_service_from_request(request)
    stream_service.del_stream(stream_name)
    return Response(status=200)

timeout_schema = Schema({
    Optional("timeout"): FloatVal(0, 60.0),
    AutoDel(STRING): object  # for all other key we don't care
})

@get_view(route_name='stream_clients')
def get_stream_clients(request):
    stream_name = request.matchdict['stream_name']
    params = get_params_from_request(request, timeout_schema)
    stream_service = get_stream_service_from_request(request)
    return stream_service.get_stream_clients(stream_name, **params)


@get_view(route_name='stream_metadata')
def get_stream_metadata(request):
    stream_name = request.matchdict['stream_name']
    params = get_params_from_request(request, timeout_schema)
    stream_service = get_stream_service_from_request(request)
    return stream_service.get_stream_metadata(stream_name, **params)


@get_view(route_name='stream_statistic')
def get_stream_statistic(request):
    stream_name = request.matchdict['stream_name']
    params = get_params_from_request(request, timeout_schema)
    stream_service = get_stream_service_from_request(request)
    return stream_service.get_stream_statistic(stream_name, **params)

@post_view(route_name='stream_key_frames')
def post_stream_key_frames(request):
    stream_name = request.matchdict['stream_name']
    params = get_params_from_request(request, timeout_schema)
    stream_service = get_stream_service_from_request(request)
    stream_service.key_frame(stream_name, **params)
    return Response(status=200)


@get_view(route_name='stream_subsequent_stream_info')
def get_stream_subsequent_stream_info(request):
    stream_name = request.matchdict['stream_name']
    params = get_params_from_request(request, timeout_schema)
    stream_service = get_stream_service_from_request(request)
    return stream_service.wait_subsequent_stream_info(stream_name, **params)



stream_op_schema = Schema({
    "op":  StrRe(r"^restart$"),   # not em
    DoNotCare(Use(STRING)): object  # for all other key we don't care
})
@post_view(route_name='stream_operations')
def post_stream_operations(request):
    stream_name = request.matchdict['stream_name']
    params = get_params_from_request(request, stream_op_schema)
    stream_service = get_stream_service_from_request(request)
    op = params["op"]
    if op == "restart":
        stream_service.restart_stream(stream_name)
    return Response(status=200)