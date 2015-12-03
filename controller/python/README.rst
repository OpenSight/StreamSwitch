Introduction
===========

About StreamSwitch
----------------------



About 


StorLever is a management, configuration, monitor system for storage & network resource of Linux, 
which provides a set of well-designed RESTful-styled web API as well as a web panel. 

The primary goal of StorLever is to ease the management of storage resources on Linux server. 
Built on top of existing Linux management tools, StorLever provides a user-friendly web panel 
for system administrators, which can reduce the learning curve and enhance the user experience for them. 

Besides web panel, StorLever offers a set of RESTful-style(HTTP+JSON) APIs to manage the Linux system remotely, 
which is another powerful feature. Based on it, the third-party management software on another host
(such as central manage system) can easily manage the remote Linux system no matter what language and platform 
it is built on. Human can pleasently understand the REST API's output as well as the computer program can interpret it easily.
Any program language has its mature library to handle HTTP protocol and interpret JSON string. SDK for StorLever 
can easily develop on any language and platform.

StorLever focus on the management of storage resource of Linux system, which is the most diverse and difficult part 
of Linux, including block device, LVM, MD, FileSystem, NAS, IP-SAN  and etc. They are usually the nightmare for system 
administrators, and often take a week or more time of them to configure these functions but not sure they are working in
the best state. StorLever is the saviour of this situation.
Through accumulating rich storage management experience and exporting a clear, simple interface, StorLever can solve these 
problem in minutes. 

StorLever is not just a frozen software, but also a extensible framework. Anyone who want to manage another object by StorLever, 
can easily develop an extension(plug-in) in a separate project without the need to change even one line of code in StorLever.

StorLever is built on the pure python so that it's simple, understandable, stable and reliable, easy to develop and deploy, 
The principle of StorLever's design is Simple, Extensible, Easy to use.
 

Highlights:
----------------

* Integrates various storage technologies of Linux,including LVM, Raid, NAS/SAN, etc.
* Provides various interface, including RESTful API, Web UI, CLI(in sub-project), SDK(in sub-project)
* Extensible, easy to add a extension(plug-in) 
* Simple, pure python


Storage Resources
-----------------

By "storage resources" we mean block devices, RAID, LVM, NAS etc.
Following is a non-exhaustive list of what is under the control of the newest StorLever:

* Block device
* SCSI device
* MD Raid
* LSI Raid (LSISAS2108, LSISAS2008 chip)
* LVM
* XFS
* EXT4
* NFS(Client & Server)
* SAMBA
* FTP
* iSCSI(Initiator & Target)

For the complete list of software packages involved, check out the dependency document.

Why StorLever
-------------------

Traditionally, Linux distribution would provides two kinds of local operation interface for 
system administrator, typical CLI and GUI. They used to be the most popular approach to manage
Linux system. As the network, especially Internet becomes universal,  Linux system usually locates
on the remote machine(especially VPS, Virtual Private Sever). Local interface does not make sense 
any more, administrator like to use SSH to login the remote system to manage them. Management with 
SSH is flexible, but difficult, painful, time-consuming. Some Web Control Panel project is developed 
to assist the administrators to fulfill this task. 
With the popularity of cloud computing, more and more VPS must be managed by the administrators, 
so that they need the assist of a central management platform to perform the management task, 
which need the simple management APIs of the remote system. 
 
StorLever is the answer of these problem.

Why Web API
-----------

All those "resources" mentioned above, come with their own utilities like command line
and configuration files for the management purpose, but unfortunately all in different style.
To master all those the command lines and configuration files requires non-trivial effort, 
and to access these utilities remotely and programmatically requires another set of skills,
which make things even worse.

So StorLever comes to the rescue. By providing a set of uniformed, well-designed web API, 
we abstract away all the configuration and command line details, with simplicity and consistency
in mind during API designing, people will find them easy to understand. 

All those API are HTTP based, which makes them totally platform and programming language independent.

Differences with other similar projects
---------------------------------------

There are already projects like openfiler which service the similar purpose, but what's
the difference?

Openfiler or freeNAS are both storage systems delivered with the whole operation system
and all the utilities sit on it, on the contrary, StorLever is only a set of API that helps the user
to ease the management of storage resources. It is based on top of existing management tools in Linux 
distribution, e.g. RHEL/CentOS. RHEL/CentOS 6/7, which is maintained by a bunch of Linux experts
and has already proven itself the most rock-solid enterprise-class Linux distribution. StorLever also depends
on the packages/RPMs delivered by RHEL/CentOS, but user has the freedom to install only the packages they
actually required. 

Ajenti is another excellent project which is also developed in Python, and its purpose to provide a web control
panel for Linux on VPS and it's also extensible. It is a general control panel including some general utilities of 
Linux, but not focus on the storage management, and also does not provides software development API to other system

OpenLMI is a very similar project to StorLever, and it has been included in the RHEL 7, which also provides APIs for 
administrators to remotely manage, configure, monitor Linux system. But we don't think it's on the right way, 
OpenLMI is based on the technology so called "WBEM" to implement its scalability (extensibility). 
WBEM is a very complex technology architecture initiated by some big enterprise like Microsoft,Compaq, Cisco at 1990s 
to support enterprise distributed computing environment. 
It is consist of many components and many protocol, flexible and considerate, but difficult to understand, 
difficult to deploy, difficult to develop. The network datagram is also difficult to read by human. 
We don't think we need such a complex architecture to implement remote management of Linux system. 
StorLever is simple framework to implement scalability, provides simple API, friendly web page. As to OpenLMI, 
StorLever is an alternative lightweight solution to fulfill the same mission. 



Install
====================

StorLever is web service designed to ease the management of various
storage resource on your CentOS/RHEL 6 server. It is based on the brilliant 
Python web framework [Pyramid](http://www.pylonsproject.org/) to build its web service, 
and make use of [PasteDeploy](http://pythonpaste.org/deploy/) system to deploy its WSGI server/app configurations

It requires Python 2.6 or higher, but Python 3k is not supported. 


Installing from source code
-------------------

Download and unzip StorLever's source package in your Linux system. StorLever can be run on RHEL/CentOS 6 or above,
for other Linux distribution(like Fedora, Ubuntu), it's not sure can be run. 

At the StorLever project's root directory, enter the following command to install StorLever into your Linux system 

	$ python setup.py install
	
This installation process would check any require project and download them from pypi. 


Configure
---------------------

StorLever make use of paste configuration system, so you should create and edit some paste config file 
before start StorLever. you can find some sample paste config file in the root directory of StorLever 
source distribution. please keep the most option values default in the file, except the server port.
Server listen port for StorLever is 6543 by default, and you can change it as you want.


Startup
---------------------

After installing StorLever successfully, you can start up StorLever's service in two way: 1) daemon mode; 2) foreground mode.

### Daemon mode:

If you want to start up Storlever's Web server at background, so that it can run alone from your console, 
you can enter the following command: 
    
    $ pserve --daemon --log-file=[log file] --pid-file=[pid file] [paste config file]

In this situation, StorLever would read paste configurations from the given [paste config file] 
(you can find some sample paste config file in the root directory of StorLever source distribution), 
store its process pid into the [pid file] for stop, 
redirect its stderr/stdout to /dev/null, and output the log to [log file]

If you want to stop the StorLever's server in daemon mode, enter the following command: 

    $ pserve --stop-daemon --pid-file=[pid file] 

[pid file] is the file contains the pid of the daemon. 
    

### Foreground mode:

If you want to debug or test StorLever, you may want to start StorLever's server at foreground mode, 
so that you can read the stdout/stderr from StorLever's Server. Enter the following command would start up StorLever's 
Server in foreground mode

   $ pserve [StorLever paste config file]

StorLever's paste config file can be chosen the ini file under StorLever project root directory. 
(you can find some sample paste config file in the root directory of StorLever source distribution)

If StorLever is running at foreground mode, just type Ctrl+C can terminate it. 

After that, StorLever is running, enjoy!!!!  


Startup On Boot
------------------------------

If you want to automatically start up StorLever on system boot, you can make use of the init.d script of StorLever
and chkconfig utility to add StorLever's service into the system rc.d directory.  Follow the guide below: 

First, you should copy the init.d script and configure file from StorLever project into your system 
/etc directory. At the StorLever's root directory, type the following commands: 

    $ cp initscripts/storlever /etc/init.d/
    $ cp storlever.ini /etc/
    
Then, Add StorLever into the system rc.d directory through "chkconfig" utility: 

    $ chkconfig --add storlever 

Then, reboot the machine, and you can see StorLever would run in daemon mode.     
	
For Developer
------------------------

If you are a developer who want to debug/develop StorLever, maybe you don't want to install StorLever in your system 
but just want to run it. You can enter the following command at the StorLever's project root directory: 

	$ python setup.py develop

This instruction would never install the StorLever in your python's site-packages directory. Instead, it just makes a link 
in your python's site-packages directory to the StorLever's project root directory. Also, this process would not 
install the init script and paste configure file into your system /etc/ directory.

Then, you can enter the following command to start up StorLever in foreground mode 

    $ pserve --reload storlever_dev.ini
	
this command can automatically reload your code when code change found, and it can printout useful debug information 
when unexpected exception raised in the code, and some other helpful functionality for code debug.


Virtualenv
----------

If you are a new comer of python, you can skip this section. 

It is recommended to use virtualenv for Python library management. Though there might
be only one Python interpreter install, virtualenv can make your system looks like having 
multiple Python installations, with each has its own set of libraries independently from others,
therefor there will never be library version conflicts for different projects.

For more about this topic, check out its official document 
[virtualenv](http://www.virtualenv.org/en/latest/)



Usage
====================

After you successfully install StorLever and start it up in your system, your can use StorLever service in two way. 
By default, StorLever listens on TCP port 6543, and you can change it in the paste ini configure file. 


Use StorLever with web panel
-----------------------------------

You can enter the following URL in your browser to get into StorLever's login Page. 

    http://[host_ip]:[port]/

[host_ip] is the IP address of the system running StorLever, [port] is the TCP port StorLever listens on. 
The login user by default is admin, password is 123456. 


Use StorLever with RESTful API
-----------------------------------

On the other hand, StorLever highlights its RESTful-style API, you can use a http tools or browser to test them.
All API URI starts with the following string, and does not need authentication by now. 

    http://[host_ip]:[port]/storlever/api/

You can refer to doc/api_ref.rst for more detail about each API's usage. 



Development
====================

StorLever make use of Github to host its latest code with the following URL:

    https://github.com/OpenSight/StorLever
	
Developers should use the issue system of Github to feedback some bug/requirement to StorLever

If Developers want to participate in StorLever's development and contribute their code, 
they should use the Fork + Pull Request mechanism against StorLever's master branch. 
If StorLever adopt your code, we would put your name in StorLever author list. Thanks for your suppport

Architecture
----------------------

We provides a architecture picture in doc/ directory to help developer to understand StorLever's structure. 

StorLever includes 3 layer which implements different functions:

* manager layer

This layer contains many manager objects, each one is responsible for a specific sub-system's management, 
such as LVM, FTP, File System, etc.
These manager objects are the core of StorLever, which locate at the bottom, 
and provides object-oriented interfaces to upper layer. 

* REST layer

This layer is responsible to output RESTful-style API to client. It get the RESTful request from clients, 
check parameters, and communicate with manager layer to get result. 
This layer make use of manager layer and provides RERTful API to upper layer. 

* Web layer

This layer is to provides the Web Panel to administrator to manage the system. It call StorLever's RESTful 
API to show the result to administrator in their browser. 
This layer make use of REST layer and provide a Web UI to administrator
We provide a specific document to describe the Web Page's design in doc/ directory.


Write an extension
-----------------------

If you want to develop an extension(plug-in) for StorLever, you should follow some convention of StroLever. 
And each extension has the same architecture with StorLever, also includes three layers. 
We would provide a How-to document to detail this topic. 
