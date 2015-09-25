"""
streamswitch.wsgiapp.daos.alchemy_dao_context_mngr
~~~~~~~~~~~~~~~~~~~~~~~

This module defines the base class of DAO context manager

:copyright: (c) 2015 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""


class DaoContext(object):
    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        pass


class DaoContextMngr(object):
    def context(self):
        pass