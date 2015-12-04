"""
streamswitch.wsgiapp.app_main
~~~~~~~~~~~~~~~~

the Controller wsgi application's main file to make a WSGI application.

:copyright: (c) 2014 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""
from __future__ import unicode_literals, division

from gevent import reinit
# from gevent import monkey;monkey.patch_all()


STORLEVER_ENTRY_POINT_GROUP = 'streamswitch.wsgiapp.extensions'


def set_services(config):
    """ constructs every service used in this WSGI application """

    from .services.stream_service import StreamService
    from .daos.port_dao import PortDao
    from .daos.stream_conf_dao import StreamConfDao
    from .daos.alchemy_dao_context_mngr import AlchemyDaoContextMngr
    from .services.port_service import PortService
    from .services.process_watcher_service import ProcessWatcherService
    from .. import stream_mngr, port_mngr, process_mngr
    from pyramid.events import ApplicationCreated
    from sqlalchemy import engine_from_config, dialects

    settings = config.get_settings()

    # create port service
    port_conf_file = settings.get("port_conf_file",
                                  "/etc/streamswitch/ports.yaml")
    port_dao = PortDao(port_conf_file)
    port_service = PortService(port_mngr=port_mngr, port_dao=port_dao)
    config.add_settings(port_service=port_service)
    config.add_subscriber(port_service.on_application_created,
                          ApplicationCreated)


    # patch the sqlite dialect to make it compatible with gevent
    dialects.registry.register("sqlite", "streamswitch.wsgiapp.utils.sqlalchemy_gevent", "SqliteDialect")


    # create stream service
    engine = engine_from_config(settings, 'sqlalchemy.')
    dao_context_mngr = AlchemyDaoContextMngr(engine)
    stream_conf_dao = StreamConfDao(dao_context_mngr)

    stream_service = StreamService(stream_mngr=stream_mngr,
                                   stream_conf_dao=stream_conf_dao,
                                   dao_context_mngr=dao_context_mngr)
    # stream_service = StreamService(stream_mngr=stream_mngr)
    config.add_settings(stream_service=stream_service)
    config.add_subscriber(stream_service.on_application_created,
                          ApplicationCreated)

    # create process watcher service
    pw_service = ProcessWatcherService(process_mngr=process_mngr)
    config.add_settings(process_watcher_service=pw_service)

    

def make_wsgi_app(global_config, **settings):
    """ This function returns a Pyramid WSGI application.
    """

    #reinit the gevent lib at first
    reinit()

    # from storlever.lib.lock import set_lock_factory_from_name
    # from storlever.lib.security import AclRootFactory
    from ..utils import CustomJSONEncoder
    from pyramid.config import Configurator
    from pyramid.renderers import JSON

    from pyramid.session import UnencryptedCookieSessionFactoryConfig
    # from pyramid.authentication import SessionAuthenticationPolicy
    # from pyramid.authorization import ACLAuthorizationPolicy

    streamswitch_session_factory = UnencryptedCookieSessionFactoryConfig('streamswitch201509')
    # storlever_authn_policy = SessionAuthenticationPolicy()
    # storlever_authz_policy = ACLAuthorizationPolicy()


    config = Configurator(session_factory=streamswitch_session_factory,
                          # root_factory=AclRootFactory,
                          # authentication_policy=storlever_authn_policy,
                          # authorization_policy=storlever_authz_policy,
                          settings=settings)

    config.add_static_view('static', 'static', cache_max_age=3600)

    # get user-specific config from setting
    json_indent = settings.get("json.indent")
    if json_indent is not None:
        json_indent = int(json_indent)
    # make JSON as the default renderer
    config.add_renderer(None, JSON(indent=json_indent, check_circular=True, cls=CustomJSONEncoder))

    # route and view configuration of REST API version 1 can be found in module storlever.rest
    # check storlever.rest.__init__.py for more detail
    config.include('streamswitch.wsgiapp.views', route_prefix='stsw/api/v1')

    # configure the services into deploy setting
    set_services(config)

    # loads all the extensions with entry point group "streamswitch.wsgiapp.extensions"
    launch_extensions(config)

    return config.make_wsgi_app()


def launch_extensions(config):
    import pkg_resources
    for entry_point in pkg_resources.iter_entry_points(group=STORLEVER_ENTRY_POINT_GROUP):
        try:
            # Grab the function that is the actual plugin.
            plugin = entry_point.load()
            # call the extension's main function
            plugin(config)
        except ImportError as e:
            pass




