"""
streamswitch.wsgiapp.daos.alchemy_dao_context_mngr
~~~~~~~~~~~~~~~~~~~~~~~

This module implements the DAO context manager of SQLAlchemy ORM

:copyright: (c) 2015 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""
from __future__ import unicode_literals, division
from sqlalchemy.orm import sessionmaker
from .dao_context_mngr import DaoContextMngr, DaoContext, \
    CONTEXT_TYPE_AUTOCOMMIT, CONTEXT_TYPE_TRANSACTION, CONTEXT_TYPE_NESTED
from gevent.local import local
from gevent.threadpool import ThreadPool

Session = sessionmaker()


class AlchemyDaoContext(DaoContext):
    def __init__(self, mngr, context_type, **kwargs):
        self._mngr = mngr
        self._context_type = context_type
        self._session_kwargs = kwargs
        self._is_session_owner = False
        self.session = None
        self.thread_pool = mngr.thread_pool

    def __enter__(self):
        if self._mngr.local.current_session is None:
            session = self._mngr.session_maker(autocommit=True,
                                               **self._session_kwargs)

            self._mngr.local.current_session = session
            self._is_session_owner = True

        self.session = self._mngr.local.current_session
        if self._type == CONTEXT_TYPE_NESTED:
            self.session.begin_nested()
        elif self._type == CONTEXT_TYPE_TRANSACTION:
            self.session.begin(subtransactions=True)

        return self

    def __exit__(self, exc_type, exc_val, exc_tb):

        if self._is_session_owner:
            self._mngr.local.current_session = None

        self._mngr.thread_pool.apply(self._exit_in_thread,
                                     (exc_type, exc_val, exc_tb))
        self.session = None
        self._is_session_owner = False

        return False

    def _exit_in_thread(self, exc_type, exc_val, exc_tb):
        try:
            if exc_val is None:
                # successful
                if self._type != CONTEXT_TYPE_AUTOCOMMIT:
                    self.session.commit()
            else:
                # exception
                if self._type != CONTEXT_TYPE_AUTOCOMMIT:
                    self.session.rollback()
        finally:
            if self._is_session_owner:
                self.session.close()



class ContextMngrLocal(local):
    current_session = None


class AlchemyDaoContextMngr(DaoContextMngr):
    def __init__(self, engine, thread_pool_size=1):
        self.session_maker = sessionmaker(bind=engine)
        self.local = ContextMngrLocal()
        self.thread_pool = ThreadPool(thread_pool_size)

    def context(self, context_type=CONTEXT_TYPE_TRANSACTION, **kwargs):
        return AlchemyDaoContext(self, context_type, **kwargs)