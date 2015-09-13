import yaml
import os

class ConfigError(Exception):
    pass


class Config(object):
    def __init__(self, conf_file, conf=None, schema=None):
        self.conf_file = conf_file
        self.conf = conf
        self.schema = schema

    def parse(self):
        if self.conf_file is not None and self.conf_file != "":
            try:
                with open(self.conf_file, "r") as f:
                    self.conf = yaml.load(f)
                if self.schema:
                    self.conf = self.schema.validate(self.conf)
                return self.conf
            except Exception as e:
                raise ConfigError(str(e))
        else:
            raise ConfigError("conf file absent")

    def write(self):
        if self.conf_file is not None and self.conf_file != "" and \
                self.conf is not None:
            try:
                with open(self.conf_file, "w") as f:
                    os.chmod(self.conf_file, 0600) # make config file is r/w only for root
                    yaml.dump(self.conf, f, default_flow_style=False)
            except Exception:
                raise ConfigError(str(Exception))
        else:
            raise ConfigError("conf file absent")

    @classmethod
    def from_file(cls, conf_file, schema=None):
        conf = cls(conf_file, schema=schema)
        conf.parse()
        return conf

    @classmethod
    def to_file(cls, conf_file, conf):
        conf = cls(conf_file, conf=conf)
        conf.write()
        return conf
