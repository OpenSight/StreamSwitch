from __future__ import unicode_literals, division
from gevent import reinit
from gevent.pywsgi import WSGIServer
import sys


def gevent_pywsgi_server_runner(wsgi_app, global_conf, host="0.0.0.0", port=8088, **kwargs):
    port = int(port)
    reinit()
    WSGIServer(listener=(host, port), application=wsgi_app, **kwargs).serve_forever()


def main(argv=sys.argv):
    pass

if __name__ == '__main__': # pragma: no cover
    sys.exit(main() or 0)