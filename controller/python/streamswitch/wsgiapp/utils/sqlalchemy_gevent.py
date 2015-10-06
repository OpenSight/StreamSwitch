"""
streamswitch.wsgiapp.utils.sqlalchemy_gevent
~~~~~~~~~~~~~~~~~~~~~~~

This module is a fork from sqlalchemy_gevent project
(https://github.com/hkwi/sqlalchemy_gevent)

but we change problems:
1) use a single thread pool for all connections of sqlite
2) make compatible with python2 and python3
3) use another thread pool for the DB dialects except sqlite

"""


# from __future__ import unicode_literals, division

from sqlalchemy.engine import default
from sqlalchemy.dialects import registry
import sqlalchemy.dialects.sqlite
import gevent
import gevent.threadpool
import traceback





class FuncProxy(object):
	def __init__(self, func, threadpool):
		self.func = func
		self.threadpool = threadpool
	
	def __call__(self, *args, **kwargs):
		return self.threadpool.apply_e(BaseException, self.func, args, kwargs)

class Proxy(object):
	_inner = None
	_context = None
	def __getattr__(self, name):
		obj = getattr(self._inner, name)
		if name in self._context.get("methods",()):
			threadpool = self._context.get("threadpool", gevent.get_hub().threadpool)
			return FuncProxy(obj, threadpool)
		else:
			return obj

class ConnectionProxy(Proxy):
	def cursor(self):
		threadpool = self._context.get("threadpool", gevent.get_hub().threadpool)
		methods = ("callproc", "close", "execute", "executemany",
			"fetchone", "fetchmany", "fetchall", "nextset", "setinputsizes", "setoutputsize")
		return type("CursorProxy", (Proxy,), {
			"_inner": threadpool.apply(self._inner.cursor, None, None),
			"_context": dict(list(self._context.items())+[("methods", methods),]) })()

class DbapiProxy(Proxy):
	def connect(self, *args, **kwargs):
		threadpool = self._context.get("threadpool", gevent.get_hub().threadpool)
		methods = ("close", "commit", "rollback", "cursor")
		return type("ConnectionProxy", (ConnectionProxy,), {
			"_inner": threadpool.apply(self._inner.connect, args, kwargs),
			"_context": dict(list(self._context.items())+[("methods", methods), ("threadpool", threadpool)]) })()

class ProxyDialect(default.DefaultDialect):
	_inner = None
	_context = None
	
	@classmethod
	def dbapi(cls):
		return type("DbapiProxy", (DbapiProxy,), {
			"_inner": cls._inner.dbapi(),
			"_context": cls._context })()

def dialect_name(*args):
	return "".join([s[0].upper()+s[1:] for s in args if s])+"Dialect"

single_thread_pool = gevent.threadpool.ThreadPool(1)
dialect_thread_pool = gevent.threadpool.ThreadPool(10)

def dialect_maker(db, driver):
	class_name = dialect_name(db, driver)
	if driver is None:
		driver = "base"
	
	dialect = __import__("sqlalchemy.dialects.%s.%s" % (db, driver), fromlist=['__name__'], level=0).dialect
	
	context = {}
	if db == "sqlite": # pysqlite dbapi connection requires single threaded
		context["threadpool"] = single_thread_pool
	else:
		context["threadpool"] = dialect_thread_pool
	
	return type(class_name,
		(ProxyDialect, dialect), {
		"_inner": dialect,
		"_context": context })

bundled_drivers = {
	"firebird":"kinterbasdb fdb".split(),
	"mssql":"pyodbc adodbapi pymssql zxjdbc mxodbc".split(),
	"mysql":"mysqldb oursql pyodbc zxjdbc mysqlconnector pymysql gaerdbms cymysql".split(),
	"oracle":"cx_oracle zxjdbc".split(),
	"postgresql":"psycopg2 pg8000 pypostgresql zxjdbc".split(),
	"sqlite":"pysqlite".split(),
	"sybase":"pysybase pyodbc".split()
	}

for db, drivers in bundled_drivers.items():
	try:
		globals()[dialect_name(db)] = dialect_maker(db, None)
		for driver in drivers:
			globals()[dialect_name(db,driver)] = dialect_maker(db, driver)
	except Exception as e:
		# drizzle was removed in sqlalchemy v1.0
		# ignore handler exception
		traceback.print_exc()
		pass

# import pprint
# pprint.pprint(globals())

def patch_all():
	for db, drivers in bundled_drivers.items():
		registry.register(db, "sqlalchemy_gevent", dialect_name(db))
		for driver in drivers:
			registry.register("%s.%s" % (db,driver), "sqlalchemy_gevent", dialect_name(db,driver))

