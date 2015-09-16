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
    Use, IntVal, Default, SchemaError, BoolVal, StrRe, ListVal, Or
from .common import get_params_from_request
import gevent


def includeme(config):
    config.add_route('stream_list', '/stream_list')
    config.add_route('stream', '/stream_list/{stream_name}')
    config.add_route('source_type_list', '/source_type_list')
    config.add_route('stream_clients', '/stream_list/{stream_name}/client_list')
    config.add_route('stream_metadata', '/stream_list/{stream_name}/metadata')
    config.add_route('stream_statistic', '/stream_list/{stream_name}/statistic')
    config.add_route('stream_key_frames', '/stream_list/{stream_name}/key_frames')


def get_stream_service_from_request(request):
    registry = request.registry
    return registry.settings['stream_service']


@get_view(route_name='source_type_list')
def get_source_type_list(request):
    stream_service = get_stream_service_from_request(request)
    return stream_service.get_source_type_list()


@get_view(route_name='stream_list')
def get_stream_list(request):
    stream_service = get_stream_service_from_request(request)
    return stream_service.get_stream_list()


@post_view(route_name='stream_list')
def add_stream(request):
    return Response(status=200)


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


@get_view(route_name='stream_clients')
def get_stream(request):
    stream_name = request.matchdict['stream_name']
    params = get_params_from_request(request)
    stream_service = get_stream_service_from_request(request)
    return stream_service.get_stream_clients(stream_name, **params)


@get_view(route_name='stream_metadata')
def get_stream(request):
    stream_name = request.matchdict['stream_name']
    params = get_params_from_request(request)
    stream_service = get_stream_service_from_request(request)
    return stream_service.get_stream_metadata(stream_name, **params)


@get_view(route_name='stream_statistic')
def get_stream(request):
    stream_name = request.matchdict['stream_name']
    params = get_params_from_request(request)
    stream_service = get_stream_service_from_request(request)
    return stream_service.get_stream_statistic(stream_name, **params)

@post_view(route_name='stream_key_frames')
def get_stream(request):
    stream_name = request.matchdict['stream_name']
    params = get_params_from_request(request)
    stream_service = get_stream_service_from_request(request)
    return stream_service.key_frame(stream_name, **params)