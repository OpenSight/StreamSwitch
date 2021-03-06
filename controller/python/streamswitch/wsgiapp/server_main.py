


from __future__ import unicode_literals, division

from gevent.pywsgi import WSGIServer


def gevent_pywsgi_server_runner(wsgi_app, global_conf, host="0.0.0.0", port=8088, **kwargs):
    port = int(port)
    WSGIServer(listener=(host, port), application=wsgi_app, **kwargs).serve_forever()

