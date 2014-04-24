
ACLOCAL_AMFLAGS = -I m4

vimtest: all

lib_LTLIBRARIES = libqiconn.la
noinst_LIBRARIES = libqiconn.a

libqiconn_la_SOURCES = qiconn.cpp
libqiconn_la_CPPFLAGS = $(AM_CPPFLAGS)

libqiconn_a_SOURCES = qiconn.cpp
libqiconn_a_CPPFLAGS = $(AM_CPPFLAGS)

doc: include/qiconn/*.h *.cpp qiconn.dox
	doxygen qiconn.dox

clean-local:
	rm -rf qiconn-doc

distclean-local:
	rm m4/*.m4


