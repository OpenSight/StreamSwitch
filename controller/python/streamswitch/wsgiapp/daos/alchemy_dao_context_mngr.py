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
    CONTEXT_TYPE_TRANSACTION, CONTEXT_TYPE_NESTED
from gevent.local import local


class AlchemyDaoContext(DaoContext):
    def __init__(self, mngr, context_type, session_kwargs):
        self._mngr = mngr
        self._context_type = context_type
        self.session = mngr.session_maker(autocommit=True,
                                          expire_on_commit=False,
                                          **session_kwargs)
        self._enter_count = 0

        # print("init session")

    def close(self):
        if self._mngr.local.context_in_active is self:
            self._mngr.local.context_in_active = None
        self.session.close()

        # print("close session")



    def __enter__(self):


        try:
            if self._context_type == CONTEXT_TYPE_NESTED:
                self.session.begin_nested()
            elif self._context_type == CONTEXT_TYPE_TRANSACTION:
                self.session.begin(subtransactions=True)
        except Exception:
            if self._enter_count == 0:
                self.close()
            raise

        self._enter_count += 1

        return self

    def __exit__(self, exc_type, exc_val, exc_tb):

        self._enter_count -= 1
        try:
            if exc_val is None:
                # successful
                self.session.commit()
            else:
                # exception
                self.session.rollback()
        finally:
            if self._enter_count == 0:
                self.close()

        return False



class ContextMngrLocal(local):
    context_in_active = None


class AlchemyDaoContextMngr(DaoContextMngr):
    def __init__(self, engine):
        self.session_maker = sessionmaker(bind=engine)
        self.local = ContextMngrLocal()


    def context(self, context_type=CONTEXT_TYPE_TRANSACTION, **kwargs):
        if self.local.context_in_active is None:
            # create a new context
            self.local.context_in_active = \
                AlchemyDaoContext(self, context_type, kwargs)
        return self.local.context_in_active