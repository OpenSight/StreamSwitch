from __future__ import unicode_literals, division
import sys
from pyramid.paster import (
    get_appsettings,
    setup_logging,
    )
from sqlalchemy import engine_from_config
from sqlalchemy.schema import MetaData
from ..wsgiapp.models import Base


def initialize_db(config_uri, options={}):

    setup_logging(config_uri)
    settings = get_appsettings(config_uri, options=options)
    engine = engine_from_config(settings, 'sqlalchemy.')
    # delete all tables

    meta = MetaData()
    meta.reflect(bind=engine)
    meta.drop_all(engine)

    # create the empty tables
    Base.metadata.create_all(engine)


def main(argv=sys.argv):
    pass

if __name__ == '__main__': # pragma: no cover
    sys.exit(main() or 0)