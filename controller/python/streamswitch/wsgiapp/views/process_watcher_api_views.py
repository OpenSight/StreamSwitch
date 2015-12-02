"""
streamswitch.wsgiapp.views.process_watcher_api_views
~~~~~~~~~~~~~~~~~~~~~~~

This module implements the view callabes of restful api for process watcher monitor

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



def includeme(config):
    config.add_route('watchers', '/process_watchers')
    config.add_route('watcher', '/process_watchers/{wid}')


def get_process_watcher_service_from_request(request):
    registry = request.registry
    return registry.settings['process_watcher_service']


@get_view(route_name='watchers')
def get_ports(request):
    pw_service = get_process_watcher_service_from_request(request)
    return pw_service.get_process_watcher_list()


@get_view(route_name='port')
def get_port(request):
    wid = int(request.matchdict['wid'])
    pw_service = get_process_watcher_service_from_request(request)
    return pw_service.get_process_watcher(wid)

