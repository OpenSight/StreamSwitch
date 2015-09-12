"""
storlever.common
~~~~~~~~~~~~~~~~

StorLever's main file to make a WSGI application.

:copyright: (c) 2014 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""

import pkg_resources

STORLEVER_ENTRY_POINT_GROUP = 'storlever.extensions'


def make_wsgi_app(global_config, **settings):
    """ This function returns a Pyramid WSGI application.
    """
    from storlever.lib.lock import set_lock_factory_from_name
    from storlever.lib.security import AclRootFactory
    from storlever.lib.utils import CustomJSONEncoder
    from pyramid.config import Configurator
    from pyramid.renderers import JSON

    from pyramid.session import UnencryptedCookieSessionFactoryConfig
    from pyramid.authentication import SessionAuthenticationPolicy
    from pyramid.authorization import ACLAuthorizationPolicy

    storlever_session_factory = UnencryptedCookieSessionFactoryConfig('storlever201308')
    storlever_authn_policy = SessionAuthenticationPolicy()
    storlever_authz_policy = ACLAuthorizationPolicy()


    config = Configurator(session_factory=storlever_session_factory,
                          root_factory=AclRootFactory,
                          authentication_policy=storlever_authn_policy,
                          authorization_policy=storlever_authz_policy,
                          settings=settings)

    config.add_static_view('static', 'static', cache_max_age=3600)

    # get user-specific config from setting
    json_indent = settings.get("json.indent")
    if json_indent is not None:
        json_indent = int(json_indent)

    # get lock factory
    if (settings.get("lock.module") is not None) and \
            (settings.get("lock.factory") is not None):
        set_lock_factory_from_name(settings.get("lock.module"),
                                   settings.get("lock.factory"))

    # make JSON as the default renderer
    config.add_renderer(None, JSON(indent=json_indent, check_circular=True, cls=CustomJSONEncoder))

    # route and view configuration of REST API version 1 can be found in module storlever.rest
    # check storlever.rest.__init__.py for more detail
    config.include('storlever.rest', route_prefix='storlever/api/v1')

    # route and view configuration of Web UI can be found in module storlever.web
    # check storlever.web.__init__.py for more detail
    config.include('storlever.web')

    # loads all the extensions with entry point group "storlever.extenstions"
    launch_extensions(config)

    return config.make_wsgi_app()


def launch_extensions(config):

    for entry_point in pkg_resources.iter_entry_points(group=STORLEVER_ENTRY_POINT_GROUP):
        try:
            # Grab the function that is the actual plugin.
            plugin = entry_point.load()
            # call the extension's main function
            plugin(config)
        except ImportError as e:
            pass


