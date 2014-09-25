dnl
dnl Qiconn Copyright (C) 2013 Jean-Daniel Pauget
dnl A whole set of http tcp sessions
dnl
dnl qiconn@nekodune.com  -  http://qiconn.nekodune.com/


dnl This file is part of Qiconn
dnl 
dnl Qiconn is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 2 of the License, or
dnl (at your option) any later version.
dnl 
dnl Qiconn is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl 
dnl You should have received a copy of the GNU General Public License
dnl along with Qiconn; if not, write to the Free Software
dnl Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
dnl 
dnl you can also try the web at http://www.gnu.org/
dnl 
dnl
dnl qiconn@nekodune.com  -  http://qiconn.nekodune.com/

dnl Process this file with autoconf to produce a configure script.
AC_INIT([qiconn], [1.0.4], [qiconn@nekodune.com])
AC_CONFIG_SRCDIR([./qiconn.cpp])

QICONN_LIB_CURRENT=0
QICONN_LIB_REVISION=0
QICONN_LIB_AGE=0

dnl Detect the canonical host and target build environment
AC_CANONICAL_HOST
AC_CANONICAL_TARGET


dnl quieten some complains from missing files
AM_INIT_AUTOMAKE([foreign])

dnl the magic of additionnal local m4 ?
AC_CONFIG_MACRO_DIR([m4])

dnl autoheader
AM_CONFIG_HEADER(config.h)

dnl Check for tools

AC_PROG_CC
AC_PROG_CXX
AC_LIBTOOL_WIN32_DLL
AM_PROG_LIBTOOL

dnl Check for compiler environment

AC_C_CONST

dnl JD's standard minimum CFLAG
dnl CHANGED for JD's ENterGroup default nasty warnings
CFLAGS="$CFLAGS -O2 -Wall -Wcast-qual -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes"
dnl CXXFLAGS="$CXXFLAGS -O2 -Wall -Woverloaded-virtual -Wcast-qual -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wredundant-decls -Winline"
CXXFLAGS="$CXXFLAGS -O2 -Wall -Woverloaded-virtual -Wcast-qual -Wwrite-strings -Wredundant-decls"

dnl ####    dnl check for qiconn sources
dnl ####    AC_MSG_CHECKING(for qiconn sources)
dnl ####    if test -f qiconn/include/qiconn/qiconn.h ; then
dnl ####        INCPATH="$INCPATH -Iqiconn/include"
dnl ####        QICONNSOURCE=qiconn
dnl ####        AC_MSG_RESULT(found)
dnl ####    else
dnl ####        AC_MSG_ERROR(Unable to find qiconn sources)
dnl ####    fi
dnl ####    AC_SUBST(QICONNSOURCE)


dnl badly look for qiconn includes ...
AC_MSG_CHECKING(for qiconn headers)
if test -f include/qiconn/qiconn.h ; then
    INCPATH="$INCPATH -Iinclude"
    AC_MSG_RESULT(found)
else
    AC_MSG_ERROR(Unable to find qiconn hearders)
fi



dnl check for TCP_WINDOW_CLAMP --------------------------------------------------------
AC_MSG_CHECKING(for TCP_WINDOW_CLAMP)
have_tcp_clamp=no
AC_TRY_COMPILE([
#include <netinet/tcp.h>
#include <arpa/inet.h>
],[
int s=0;
int clamp = 1024;
setsockopt (s, IPPROTO_TCP, TCP_WINDOW_CLAMP, &clamp, sizeof(clamp));
],[
have_tcp_clamp=yes
])
AC_MSG_RESULT($have_tcp_clamp)
if test x$have_tcp_clamp = xyes; then
    AC_DEFINE(HAVE_TCP_WINDOW_CLAMP, 1, [Define to 1 if you have TCP_WINDOW_CLAMP])
else
    AC_DEFINE(HAVE_TCP_WINDOW_CLAMP, 0, [Define to 1 if you have TCP_WINDOW_CLAMP])
fi

dnl check for SOL_TCP and TCP_DEFER_ACCEPT --------------------------------------------
AC_MSG_CHECKING(for SOL_TCP and TCP_DEFER_ACCEPT)
have_sol_tcp=no
AC_TRY_COMPILE([
#include <netinet/tcp.h>
#include <arpa/inet.h>
],[
int s=0;
int yes = 1;
setsockopt (s, SOL_TCP, TCP_DEFER_ACCEPT, &yes, sizeof(yes));
],[
have_sol_tcp=yes
])
AC_MSG_RESULT($have_sol_tcp)
if test x$have_sol_tcp = xyes; then
    AC_DEFINE(HAVE_SOL_TCP, 1, [Define to 1 if you have SOL_TCP and TCP_DEFER_ACCEPT])
else
    AC_DEFINE(HAVE_SOL_TCP, 0, [Define to 1 if you have SOL_TCP and TCP_DEFER_ACCEPT])
fi

dnl check for TCP_CORK ----------------------------------------------------------------
AC_MSG_CHECKING(for TCP_CORK)
have_tcp_cork=no
AC_TRY_COMPILE([
#include <netinet/tcp.h>
#include <arpa/inet.h>
],[
int s=0;
int yes = 1;
setsockopt (s, IPPROTO_TCP, TCP_CORK, &yes, sizeof(yes));
],[
have_tcp_cork=yes
])
AC_MSG_RESULT($have_tcp_cork)
if test x$have_tcp_cork = xyes; then
    AC_DEFINE(HAVE_TCP_CORK, 1, [Define to 1 if you have TCP_CORK])
else
    AC_DEFINE(HAVE_TCP_CORK, 0, [Define to 1 if you have TCP_CORK])
fi


dnl check for mac-osX file scheme
case "$target" in
    *-apple-darwin*)
	AC_DEFINE(OSXFILESCHEME, 1, [a macosX file scheme flavour is used])
	AC_MSG_RESULT([a macosX file scheme flavour is used])
	;;
    *)
	AC_DEFINE(OSXFILESCHEME, 0, [a regular unix file scheme flavour is used])
	AC_MSG_RESULT([a regular unix file scheme flavour is used])
	;;
esac

dnl export libqiconn versionning numbers
LIBQICONN_LDFLAGS="-version-info ${QICONN_LIB_CURRENT}:${QICONN_LIB_REVISION}:${QICONN_LIB_AGE}"
AC_SUBST(LIBQICONN_LDFLAGS)

dnl concatenation of include dirs
CFLAGS="$CFLAGS $INCPATH"
CXXFLAGS="$CXXFLAGS $INCPATH"

dnl Finally create all the generated files
AC_OUTPUT([
	Makefile
])


