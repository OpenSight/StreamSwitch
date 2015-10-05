from __future__ import unicode_literals, division, print_function
import sys
from pyramid.paster import (
    get_appsettings,
    setup_logging,
    )
from sqlalchemy import engine_from_config
from sqlalchemy.schema import MetaData
from streamswitch.wsgiapp.models import Base
import argparse
from streamswitch.exceptions import StreamSwitchError
import os
import stat
import shutil
from pkg_resources import resource_filename
from alembic.config import Config
from alembic import command

STREAMSWITCH_CONF_DIR = "/etc/streamswitch"


def initialize_db(config_uri, options={}):

    setup_logging(config_uri)
    settings = get_appsettings(config_uri, options=options)
    engine = engine_from_config(settings, 'sqlalchemy.')
    # delete all tables

    meta = MetaData()
    meta.reflect(bind=engine)
    meta.drop_all(engine)


    upgrade_db(config_uri)


def upgrade_db(config_uri):
    alembic_cfg = Config(config_uri)
    command.upgrade(alembic_cfg, "head")


class DeloyCommand(object):


    description = """
This command is a tools to deploy the web application of StreamSwitch controller
which provides RESTful-style API to manage the streams
This program provides various function command for deloy, using

    stsw_web_deploy.py sub-command -h

to print the options description for each command
    """
    def __init__(self):


        self.parser = argparse.ArgumentParser(description=self.description,
                                              formatter_class=argparse.RawDescriptionHelpFormatter)
        subparsers = self.parser.add_subparsers(help='sub-command help',
                                                title='subcommands',
                                                description='valid subcommands')

        parser = subparsers.add_parser('install',
                                       description='copy related files of the web '
                                         'application to your system, including configuration files, init scripts and so on, '
                                         ' then upgrade the specified DB to the latest schema version. ',
                                        help='copy related files of the web '
                                         'application to your system')
        parser.set_defaults(func=self.install)
        parser.add_argument("-f", "--force", action='store_true',
                            help="by default, stsw_web_deploy would not override the files if it exists "
                                                                       "unless this options is specified")
        parser.add_argument("--no-upgrade", action='store_true',
                            help="by default, stsw_web_deploy would upgrade the "
                                 "web db specified in configure file when init."
                                 "If this option is given, the upgrade operation would be ignore ")

        parser = subparsers.add_parser('uninstall',
                                       description='remove all the installed '
                                             'files from your system',
                                        help='remove all the installed '
                                             'files from your system')
        parser.set_defaults(func=self.uninstall)

        parser = subparsers.add_parser('initdb',
                                       description='initialize the SQLite db (remove any data) '
                                             'in your system',
                                        help='initialize the SQLite db (remove any data) '
                                             'in your system')
        parser.add_argument("-i", "--ini-file", help="the ini configuration file of streamswitch web app, "
                                                     "default is /etc/streamswitch/streamswitch.ini",
                            default="/etc/streamswitch/streamswitch.ini")
        parser.set_defaults(func=self.initdb)

        parser = subparsers.add_parser('upgradedb',
                                       description='upgrade the SQLite db inf your system to '
                                             'the latest schema, and keep the orignal db data',
                                        help='upgrade the SQLite db inf your system to '
                                             'the latest schema, and keep the orignal db data')
        parser.add_argument("-i", "--ini-file", help="the ini configuration file of streamswitch web app, "
                                                     "default is /etc/streamswitch/streamswitch.ini",
                            default="/etc/streamswitch/streamswitch.ini")
        parser.set_defaults(func=self.upgradedb)

        self.args = self.parser.parse_args()

    def install(self, args):
        # print(args)

        # create dir STREAMSWITCH_CONF_DIR
        if not os.path.isdir(STREAMSWITCH_CONF_DIR):
            if os.path.exists(STREAMSWITCH_CONF_DIR):
                raise StreamSwitchError("streamswitch conf path(%s) is not a directory" % STREAMSWITCH_CONF_DIR)
            else:
                print("Create dir %s." % STREAMSWITCH_CONF_DIR, end='......')
                sys.stdout.flush()
                os.makedirs(STREAMSWITCH_CONF_DIR)
                print("Done")

        # copy streamswitch.ini
        conf_file_dest = os.path.join(STREAMSWITCH_CONF_DIR, "streamswitch.ini")
        if not os.path.exists(conf_file_dest) \
            or args.force:
            print("Install file %s " % conf_file_dest, end="......")
            sys.stdout.flush()
            src_file = resource_filename("streamswitch.wsgiapp", "conf/streamswitch.ini")
            shutil.copy(src_file, conf_file_dest)
            os.chmod(conf_file_dest,
                     stat.S_IREAD| stat.S_IWRITE \
                     | stat.S_IRGRP | stat.S_IWGRP | stat.S_IROTH)
            print("Done")

        # copy ports.yaml
        ports_file_dest = os.path.join(STREAMSWITCH_CONF_DIR, "ports.yaml")
        if not os.path.exists(ports_file_dest) \
            or args.force:
            print("Install file %s " % ports_file_dest, end="......")
            sys.stdout.flush()
            src_file = resource_filename("streamswitch.wsgiapp", "conf/ports.yaml")
            shutil.copy(src_file, ports_file_dest)
            os.chmod(ports_file_dest,
                     stat.S_IREAD| stat.S_IWRITE \
                     | stat.S_IRGRP | stat.S_IWGRP | stat.S_IROTH | stat.S_IWOTH)
            print("Done")

        # upgrade db
        if not args.no_upgrade:
            settings = get_appsettings(conf_file_dest)
            print("Upgrade DB %s " % settings["sqlalchemy.url"], end="......\n")
            upgrade_db(conf_file_dest)
            print("Done")


    def uninstall(self, args):

        # remove etc conf dir
        if os.path.exists(STREAMSWITCH_CONF_DIR):
            print("Remove %s " % STREAMSWITCH_CONF_DIR, end="......")
            sys.stdout.flush()
            shutil.rmtree(STREAMSWITCH_CONF_DIR)
            print("Done")

    def initdb(self, args):
        # print(args)
        ini_conf_file = args.ini_file
        settings = get_appsettings(ini_conf_file)
        print("Initialize DB %s " % settings["sqlalchemy.url"], end="......\n")
        initialize_db(ini_conf_file)
        print("Done")

    def upgradedb(self, args):
        # print(args)
        ini_conf_file = args.ini_file
        settings = get_appsettings(ini_conf_file)
        print("Upgrade DB %s " % settings["sqlalchemy.url"], end="......\n")
        upgrade_db(ini_conf_file)
        print("Done")


    def run(self):

        if "func" not in self.args:
            self.parser.print_help()
            sys.exit(-1)

        return self.args.func(self.args)


def main(argv=sys.argv):
    command = DeloyCommand()
    return command.run()


if __name__ == '__main__': # pragma: no cover
    sys.exit(main() or 0)