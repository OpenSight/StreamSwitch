"""
streamswitch.wsgiapp.views.stream_api_views
~~~~~~~~~~~~~~~~~~~~~~~

This module implements the view callabes of restful api for stream management

:copyright: (c) 2015 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""


from .common import (get_view, post_view,
                                   put_view, delete_view)
from ...exception import StreamSwitchError

from pyramid.response import Response
from storlever.lib.schema import Schema, Optional, DoNotCare, \
    Use, IntVal, Default, SchemaError, BoolVal, StrRe, ListVal, Or
from .common import get_params_from_request

def includeme(config):
####md rest
    config.add_route('md_list', '/block/md_list') #post get 
    config.add_route('md', '/block/md_list/{md_name}')#get put delete
    config.add_route('md_op', '/block/md_list/{md_name}/op')



#http://192.168.1.10:6543/storlever/api/v1/block/md_list
@get_view(route_name='md_list')
def get_md_list_rest(request):
    md_mgr =  md.md_mgr()
    mds = md_mgr.get_all_md()  
    mds_info = mds.raid_list
    mds_dict = []
    for a_md in mds_info:
        mds_dict.append(mds_info[a_md])
    return mds_dict


#http://192.168.1.10:6543/storlever/api/v1/block/md_list/name
@get_view(route_name='md')
def get_md_rest(request):
    md_mgr =  md.md_mgr()
    mds = md_mgr.get_all_md()
    name = request.matchdict['md_name']
    md_inf = mds.get_md(name)
    return md_inf

add_md_schema = Schema({
    "name": StrRe(r"^(.+)$"),
    "level":  Or(1,0,5,10,6),
    "dev": Default(ListVal(StrRe(r"^(/dev/sd[a-z]|/dev/xvd.+)$")), default=[]),
    DoNotCare(Use(str)): object  # for all those key we don't care
})
#curl -v -X POST -d name=test -d dev=/dev/sdb,/dev/sdc -d level=1  http://192.168.1.2:6543/storlever/api/v1/block/md_list
@post_view(route_name='md_list')
def add_md_rest(request):
    md_mgr =  md.md_mgr()
    mds = md_mgr.get_all_md()
    params = get_params_from_request(request, add_md_schema)
    mds.create(params['name'],params['level'],params['dev'])
    return Response(status=200)

#curl -v -X delete http://192.168.1.123:6543/storlever/api/v1/block/md_list/name
@delete_view(route_name='md')
def delete_md_rest(request):
    md_mgr =  md.md_mgr()
    mds = md_mgr.get_all_md()
    name = request.matchdict['md_name']
    mds.delete(name)
    return Response(status=200)

md_op_schema = Schema({
    "opt": StrRe(r"^(refresh|remove|add|grow)$"),
    Optional("dev"): StrRe(r"^(/dev/sd[a-z]|/dev/vxd[a-z])$"),
    Optional("sum"): IntVal(),
    DoNotCare(Use(str)): object  # for all those key we don't care
})
#curl -v -X post -d opt=refresh http://192.168.1.123:6543/storlever/api/v1/block/md_list/name/opt
@post_view(route_name='md_op')
def post_md_op(request):
    params = get_params_from_request(request, md_op_schema)
    md_name = request.matchdict['md_name']
    mds_mgr =  md.md_mgr()
    mds = mds_mgr.get_all_md()
    md_mgr = mds.get_md(md_name)
    
    if(params['opt']) == 'refresh' :
        md_mgr.refresh()
    elif(params['opt']) == 'remove' :
        md_mgr.remove_component(params['dev'])
    elif(params['opt']) == 'add' :
        md_mgr.add_component(params['dev'])
    elif(params['opt']) == 'grow' :
        md_mgr.grow_raid(params['sum'])
        
    return Response(status=200)