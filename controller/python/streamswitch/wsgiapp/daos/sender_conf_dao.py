"""
streamswitch.wsgiapp.daos.sender_conf_dao
~~~~~~~~~~~~~~~~~~~~~~~

This module implements the DAO for stream onfiguration's persistence

:copyright: (c) 2015 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""


from __future__ import unicode_literals, division
from ..models import SenderConf


class SenderConfDao(object):
    def __init__(self, dao_context_mngr):
        self._dao_context_mngr = dao_context_mngr

    def get_all_sender_conf(self):
        with self._dao_context_mngr.context() as context:
            # in a transaction
            session = context.session
            return session.query(SenderConf).all()


    def add_sender_conf(self, sender_conf):
        with self._dao_context_mngr.context() as context:
            # in a transaction
            session = context.session
            session.add(sender_conf)


    def del_sender_conf(self, sender_name):
        with self._dao_context_mngr.context() as context:
            # in a transaction
            session = context.session
            sender_conf = session.query(SenderConf).filter(SenderConf.sender_name == sender_name).one()
            # print(sender_conf.sender_name)
            session.delete(sender_conf)
            # raise Exception("test")

def test_main():
    from .alchemy_dao_context_mngr import AlchemyDaoContextMngr
    from sqlalchemy import create_engine, dialects
    import gevent
    dialects.registry.register("sqlite", "streamswitch.wsgiapp.utils.sqlalchemy_gevent", "SqliteDialect")
    engine = create_engine("sqlite:////etc/streamswitch/stsw_web.db",echo=True)

    dao_context_mngr = AlchemyDaoContextMngr(engine)
    sender_conf_dao = SenderConfDao(dao_context_mngr)
    # import pdb
    # pdb.set_trace()
    sender_conf_list = sender_conf_dao.get_all_sender_conf()
    print("sender confs at begin:")
    print(sender_conf_list)
    sender_conf = SenderConf("test_type", "test_name", "test://test", stream_name="test_stream")
    # sender_conf_dao.del_sender_conf("test_name")
    sender_conf_dao.add_sender_conf(sender_conf)
    sender_conf_list = sender_conf_dao.get_all_sender_conf()
    print("sender confs after add:")
    print(sender_conf_list)
    sender_conf_dao.del_sender_conf("test_name")
    sender_conf_list = sender_conf_dao.get_all_sender_conf()
    print("sender confs after del:")
    print(sender_conf_list)


