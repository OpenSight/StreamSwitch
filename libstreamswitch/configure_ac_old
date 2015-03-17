#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([libstreamswitch], [0.1.0], [zqwangjk@sina.com])
AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE
AM_SILENT_RULES

LT_PREREQ([2.2])
AC_PROG_LIBTOOL


AC_CONFIG_SRCDIR([src/stsw_global.cc])
# AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_MAKE_SET
# AC_PROG_RANLIB
AM_PROG_CC_C_O

# Checks for libraries.

AC_CHECK_LIB([pthread], [pthread_create])

export PKG_CONFIG_PATH=/usr/lib/pkgconfig:/usr/lib64/pkgconfig:/lib/pkgconfig:/lib64/pkgconfig:/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig

PKG_CHECK_MODULES(zeromq, [libczmq >= 3.0.0 libzmq >= 4.0.0])
AC_SUBST(zeromq_LIBS)
AC_SUBST(zeromq_CFLAGS)

PKG_CHECK_MODULES(protobuf, [protobuf >= 2.0.0])
AC_SUBST(protobuf_LIBS)
AC_SUBST(protobuf_CFLAGS)

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h stdint.h stdlib.h string.h sys/time.h unistd.h czmq.h zmq.h])


# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

# Checks for library functions.
AC_CHECK_FUNCS([dup2 gettimeofday memset strtol])

AC_CONFIG_FILES([Makefile samples/Makefile])
AC_OUTPUT
