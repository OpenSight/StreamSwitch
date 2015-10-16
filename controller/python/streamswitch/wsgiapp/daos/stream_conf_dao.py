"""
streamswitch.wsgiapp.daos.stream_conf_dao
~~~~~~~~~~~~~~~~~~~~~~~

This module implements the DAO for stream onfiguration's persistence

:copyright: (c) 2015 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""


from __future__ import unicode_literals, division
from ..models import StreamConf


class StreamConfDao(object):
    def __init__(self, dao_context_mngr):
        self._dao_context_mngr = dao_context_mngr

    def get_all_stream_conf(self):
        with self._dao_context_mngr.context() as context:
            # in a transaction
            session = context.session
            return session.query(StreamConf).all()


    def add_stream_conf(self, stream_conf):
        with self._dao_context_mngr.context() as context:
            # in a transaction
            session = context.session
            session.add(stream_conf)


    def del_stream_conf(self, stream_name):
        with self._dao_context_mngr.context() as context:
            # in a transaction
            session = context.session
            stream_conf = session.query(StreamConf).filter(StreamConf.stream_name == stream_name).one()
            # print(stream_conf.stream_name)
            session.delete(stream_conf)
            # raise Exception("test")

def test_main():
    from .alchemy_dao_context_mngr import AlchemyDaoContextMngr
    from sqlalchemy import create_engine, dialects
    import gevent
    dialects.registry.register("sqlite", "streamswitch.wsgiapp.utils.sqlalchemy_gevent", "SqliteDialect")
    engine = create_engine("sqlite:////etc/streamswitch/stsw_web.db",echo=True)

    dao_context_mngr = AlchemyDaoContextMngr(engine)
    stream_conf_dao = StreamConfDao(dao_context_mngr)
    # import pdb
    # pdb.set_trace()
    stream_conf_list = stream_conf_dao.get_all_stream_conf()
    print("stream confs at begin:")
    print(stream_conf_list)
    stream_conf = StreamConf("test_type", "test_name", "test://test")
    # stream_conf_dao.del_stream_conf("test_name")
    stream_conf_dao.add_stream_conf(stream_conf)
    stream_conf_list = stream_conf_dao.get_all_stream_conf()
    print("stream confs after add:")
    print(stream_conf_list)
    stream_conf_dao.del_stream_conf("test_name")
    stream_conf_list = stream_conf_dao.get_all_stream_conf()
    print("stream confs after del:")
    print(stream_conf_list)


