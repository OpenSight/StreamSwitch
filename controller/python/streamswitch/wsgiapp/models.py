"""
streamswitch.wsgiapp.models
~~~~~~~~~~~~~~~~~~~~~~~

This module implements the domain models for the WSGI application,
based on the SQLAlchemy's ORM

:copyright: (c) 2015 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""
from __future__ import unicode_literals, division
from sqlalchemy import Column, Integer, String, Float, Text, \
    orm
from sqlalchemy.ext.declarative import declarative_base
from ..stream_mngr import DEFAULT_LOG_ROTATE, DEFAULT_LOG_SIZE
from ..utils import STRING, encode_json
import json


Base = declarative_base()


class StreamConf(Base):
    """ The SQLAlchemy declarative model class for a Stream config object. """
    __tablename__ = 'stream_confs'


    source_type = Column(String(convert_unicode=True), nullable=False)
    stream_name = Column(String(convert_unicode=True), primary_key=True, nullable=False)
    url = Column(String(convert_unicode=True), nullable=False)
    api_tcp_port = Column(Integer, server_default="0", nullable=False)
    log_file = Column(String(convert_unicode=True))
    log_size = Column(Integer, server_default="1048576", nullable=False)
    log_rotate = Column(Integer, server_default="3", nullable=False)
    err_restart_interval = Column(Float, server_default="30.0", nullable=False)
    age_time = Column(Float, server_default="0.0", nullable=False)
    extra_options_json = Column(Text(convert_unicode=True), default="{}", nullable=False)
    other_kwargs_json = Column(Text(convert_unicode=True), default="{}", nullable=False)

    def __init__(self, source_type, stream_name, url, api_tcp_port=0, log_file=None, log_size=DEFAULT_LOG_SIZE,
                 log_rotate=DEFAULT_LOG_ROTATE, err_restart_interval=30.0, age_time=0.0, extra_options={}, **kwargs):

        # config
        self.stream_name = STRING(stream_name)
        self.source_type = STRING(source_type)
        self.url = STRING(url)
        self.api_tcp_port = int(api_tcp_port)
        if log_file is not None:
            self.log_file = STRING(log_file)
        else:
            self.log_file = None
        self.log_size = int(log_size)
        self.log_rotate = int(log_rotate)
        self.err_restart_interval = float(err_restart_interval)
        self.age_time = float(age_time)
        self.extra_options = dict(extra_options)
        self.extra_options_json = STRING(encode_json(self.extra_options))
        self.other_kwargs = dict(kwargs)
        self.other_kwargs_json = STRING(encode_json(self.other_kwargs))

    @orm.reconstructor
    def init_on_load(self):
        self.extra_options = json.loads(self.extra_options_json)
        self.other_kwargs = json.loads(self.other_kwargs_json)

        # print("init_on_load")

    def __repr__(self):
        return "StreamConf Object(stream_name:%s, source_type:%s, " \
               "url:%s, api_tcp_port:%d)" % (
            self.stream_name, self.source_type, self.url, self.api_tcp_port
        )