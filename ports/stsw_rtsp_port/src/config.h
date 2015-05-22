/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Define this if ATTR_UNUSED is not supported */
#ifdef __WIN32__
#define ATTR_UNUSED 
#else
#define ATTR_UNUSED 
#endif

/* Debug enabled */
#undef ENABLE_DUMA

/* Define default directory for Feng A/V resources */
#undef FENICE_AVROOT_DIR_DEFAULT

/* Define default directory string for Feng A/V resources */
#undef FENICE_AVROOT_DIR_DEFAULT_STR

/* Define default directory for Feng configuration */
#undef FENICE_CONF_DIR_DEFAULT

/* Define default file for Feng configuration */
#undef FENICE_CONF_FILE_DEFAULT

/* Define absolute path string for Feng configuration file */
#undef FENICE_CONF_PATH_DEFAULT_STR

/* Define default file for Feng logger */
#undef FENICE_LOG_FILE_DEFAULT

/* Define default string for Feng log file */
#undef FENICE_LOG_FILE_DEFAULT_STR

/* Define max number of RTSP incoming sessions for Feng */
#undef FENICE_MAX_SESSION_DEFAULT

/* Define default RTSP listening port */
#undef FENICE_RTSP_PORT_DEFAULT

/* Define default dir for Certificate (pem format) */
#undef FENICE_STATE_DIR

/* Define default string dir for Certificate (pem format) */
#undef FENIC_STATE_DIR_STR

/* Define if libavformat support is available */
#undef HAVE_AVFORMAT

/* Define if libavutil support is available */
#undef HAVE_AVUTIL

/* Define this if you have clock_gettime */
#undef HAVE_CLOCK_GETTIME

/* Define to 1 if you have the <ev.h> header file. */
#undef HAVE_EV_H

/* Define to 1 if you have the <inttypes.h> header file. */
#undef HAVE_INTTYPES_H

/* Define to 1 if you have the `ev' library (-lev). */
#undef HAVE_LIBEV

/* Define to 1 if you have the `mudflapth' library (-lmudflapth). */
#undef HAVE_LIBMUDFLAPTH

/* Define to 1 if you have the <memory.h> header file. */
#undef HAVE_MEMORY_H

/* Define this if metadata support must be included */
#undef HAVE_METADATA

/* Define to 1 if MySQL libraries are available */
#undef HAVE_MYSQL

/* Define this if you have libsctp */
#undef HAVE_SCTP

/* Define to 1 if you have the <stdint.h> header file. */
#undef HAVE_STDINT_H

/* Define to 1 if you have the <stdlib.h> header file. */
#undef HAVE_STDLIB_H

/* Define to 1 if you have the <strings.h> header file. */
#undef HAVE_STRINGS_H

/* Define to 1 if you have the <string.h> header file. */
#undef HAVE_STRING_H

/* Define to 1 if the system has the type `struct sockaddr_storage'. */
#undef HAVE_STRUCT_SOCKADDR_STORAGE

/* Define to 1 if you have the <syslog.h> header file. */
#undef HAVE_SYSLOG_H

/* Define to 1 if you have the <sys/socket.h> header file. */
#undef HAVE_SYS_SOCKET_H

/* Define to 1 if you have the <sys/stat.h> header file. */
#undef HAVE_SYS_STAT_H

/* Define to 1 if you have the <sys/types.h> header file. */
#undef HAVE_SYS_TYPES_H

/* Define to 1 if you have the <unistd.h> header file. */
#undef HAVE_UNISTD_H

/* Define IPv6 support */
#undef IPV6

/* Define this if live streaming is supported */
#undef LIVE_STREAMING

/* Name of package */
#define PACKAGE   "StreamSwitch"

/* Define to the address where bug reports for this package should be sent. */
#undef PACKAGE_BUGREPORT

/* Define to the full name of this package. */
#undef PACKAGE_NAME

/* Define to the full name and version of this package. */
#undef PACKAGE_STRING

/* Define to the one symbol short name of this package. */
#undef PACKAGE_TARNAME

/* Define to the version of this package. */
#undef PACKAGE_VERSION

/* Define to 1 if you have the ANSI C header files. */
#undef STDC_HEADERS

/* Define this if the compiler supports __attribute__(( ifelse([], ,
   [destructor], []) )) */
#undef SUPPORT_ATTRIBUTE_DESTRUCTOR

/* Define this if the compiler supports __attribute__(( ifelse([], , [unused],
   []) )) */
#undef SUPPORT_ATTRIBUTE_UNUSED

/* Trace enabled */
#undef TRACE

/* Enable extensions on AIX 3, Interix.  */
#ifndef _ALL_SOURCE
# undef _ALL_SOURCE
#endif
/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# undef _GNU_SOURCE
#endif
/* Enable threading extensions on Solaris.  */
#ifndef _POSIX_PTHREAD_SEMANTICS
# undef _POSIX_PTHREAD_SEMANTICS
#endif
/* Enable extensions on HP NonStop.  */
#ifndef _TANDEM_SOURCE
# undef _TANDEM_SOURCE
#endif
/* Enable general extensions on Solaris.  */
#ifndef __EXTENSIONS__
# undef __EXTENSIONS__
#endif


/* Version number of package */
#define VERSION    "0.1.0"

/* Define to 1 if on MINIX. */
#undef _MINIX

/* Define to 2 if the system does not provide POSIX.1 features except with
   this defined. */
#undef _POSIX_1_SOURCE

/* Define to 1 if you need to in order for `stat' and other things to work. */
#undef _POSIX_SOURCE

#if !defined(NDEBUG) && defined(SUPPORT_ATTRIBUTE_DESTRUCTOR)
	   # define CLEANUP_DESTRUCTOR __attribute__((__destructor__))
	   #endif
	  
