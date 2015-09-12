
"""
streamswitch.utils.utils
~~~~~~~~~~~~~~~~

This module provide some utils function for streamswitch

:copyright: (c) 2014 by OpenSight (www.opensight.cn).
:license: AGPLv3, see LICENSE for more details.

"""
from __future__ import unicode_literals, division
import json
import sys
import shutil
import functools

if sys.version_info[:2] < (3, 3):
    from distutils.spawn import find_executable
else:
    def find_executable(name):
        return shutil.which(name)



class CustomJSONEncoder(json.JSONEncoder):
    def __init__(self, *args, **kwargs):
        # dirty hack to keep 'default' method intact
        kwargs.pop('default', None)
        super(CustomJSONEncoder, self).__init__(*args, **kwargs)

    def default(self, o):
        try:
            return json.JSONEncoder.default(self, o)
        except TypeError:
            if isinstance(o, set):
                return list(o)
            elif hasattr(o, '__json__'):
                return o.__json__()
            elif hasattr(o, '__dict__'):
                obj_dict = {}
                for k, v in o.__dict__.items():
                    if not k.startswith('_'):
                        obj_dict[k] = v
                return obj_dict


def encode_json(o):
    return json.dumps(o, check_circular=True, cls=CustomJSONEncoder)


def is_str(s):
    if sys.version_info[:1] < 3:
        isinstance(s, unicode)
    else:
        isinstance(s, str)


def import_method(path):

    if ":" in path:
        # some.module:some.attribute format
        module_path, dummy_sep, attr_path = path.partition(":")
        module = __import__(module_path, fromlist=['__name__'], level=0)
        attrs = attr_path.split('.')
    else:
        # some.module.some.attribute format
        parts = path.split('.')
        module = None
        parts_copy = parts[:]
        while parts_copy:
            try:
                module = __import__('.'.join(parts_copy))
                break
            except ImportError:
                del parts_copy[-1]
                if not parts_copy:
                    raise
        attrs = parts[1:]

    return functools.reduce(getattr, attrs, module)
