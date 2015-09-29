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
    def __init__(self, mngr, type, **kwargs):
        self._mngr = mngr
        self._type = type
        self._session_kwargs = kwargs
        self._is_session_owner = False

    @property
    def session(self):
        return self._mngr.local.current_session

    @property
    def thread_pool(self):
        return self._mngr.thread_pool

    def __enter__(self):
        if self._mngr.local.current_session is None:
            session = self._mngr.session_maker(autocommit=True,
                                                **self._session_kwargs)

            self._mngr.local.current_session = session
            self._is_session_owner = True

        if self._type == CONTEXT_TYPE_NESTED:
            self._mngr.local.current_session.begin(subtransactions=True, nested=True)
        elif self._type == CONTEXT_TYPE_TRANSACTION:
            self._mngr.local.current_session.begin(subtransactions=True, nested=False)

        return self

    def __exit__(self, exc_type, exc_val, exc_tb):

        try:
            if exc_val is None:
                # successful
                if self._type == CONTEXT_TYPE_NESTED or \
                    self._type == CONTEXT_TYPE_TRANSACTION:
                    self._mngr.local.current_session.commit()
            else:
                if self._type == CONTEXT_TYPE_NESTED or \
                    self._type == CONTEXT_TYPE_TRANSACTION:
                    self._mngr.local.current_session.rollback()
        finally:
            if self._is_session_owner:
                self._mngr.local.current_session.close()
                self._is_session_owner = False
        return False

class ContextMngrLocal(local):
    current_session = None

class AlchemyDaoContextMngr(DaoContextMngr):
    def __init__(self, engine):
        self.session_maker = sessionmaker(bind=engine)
        self.local = ContextMngrLocal()
        self.thread_pool = ThreadPool(1)


    def context(self, type=CONTEXT_TYPE_TRANSACTION, **kwargs):
        pass