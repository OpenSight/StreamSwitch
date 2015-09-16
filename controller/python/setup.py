import os
import sys

from setuptools import setup, find_packages

here = os.path.abspath(os.path.dirname(__file__))
README = open(os.path.join(here, 'README.rst')).read()
CHANGES = open(os.path.join(here, 'CHANGES.rst')).read()

requires = [
    'pyramid>=1.5.2',
    'pyramid_debugtoolbar>=2.2.2',
    'PyYaml>=3.11',
    'pyramid_chameleon>=0.3',
    'gevent>=1.1b1',
    'pyzmq>=14.0',
    'protobuf>=3.0.0a3',
]

if sys.version_info < (2,7):
    requires.append('unittest2')



setup(name='streamswitch',
      version='0.1.0',
      license='AGPLv3',
      url='https://github.com/OpenSight/StreamSwitch',
      description='Controller of StreamSwitch project (a opensource stream media server framework),  '
                  'which provides API in Python and RESTful-style service',
      long_description=README + '\n\n' + CHANGES,
      classifiers=[
        "Development Status :: 4 - Beta",
        "Framework :: Pyramid",
        "Intended Audience :: System Administrators",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: GNU Affero General Public License v3",
        "Operating System :: POSIX :: Linux",
        "Programming Language :: Python",
        "Programming Language :: Python :: 2.6",
        "Programming Language :: Python :: 2.7",
        "Programming Language :: Python :: 3.4",    
        "Topic :: Multimedia :: Video",        
        "Topic :: Multimedia :: Video :: Conversion",        
        "Topic :: Internet :: WWW/HTTP",
        "Topic :: Internet :: WWW/HTTP :: WSGI :: Application",
      ],
      author='OpenSight studio',
      author_email='zqwangjk@sina.com',
      keywords='media stream video realtime',
      packages=find_packages(),
      include_package_data=True,
      zip_safe=False,
      install_requires=requires,
      tests_require=requires, 
      entry_points="""\
      [paste.app_factory]
      main = streamswitch.wsgiapp.app_main:make_wsgi_app
      [paste.server_runner]
      gevent = streamswitch.scripts.stsw_deploy_web:gevent_pywsgi_server_runner
      """,
      )
