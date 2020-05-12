
ACLOCAL_AMFLAGS = -I m4

vimtest: all

nobase_include_HEADERS = qiconn/qiconn.h
lib_LTLIBRARIES = libqiconn.la
noinst_LIBRARIES = libqiconn.a

libqiconn_la_SOURCES = qiconn.cpp qiconn/qiconn.h
libqiconn_la_CPPFLAGS = $(AM_CPPFLAGS)

libqiconn_a_SOURCES = qiconn.cpp qiconn/qiconn.h
libqiconn_a_CPPFLAGS = $(AM_CPPFLAGS)

doc: include/qiconn/*.h *.cpp qiconn.dox
	doxygen qiconn.dox

clean-local:
	rm -rf qiconn-doc

distclean-local:
	rm -f m4/*.m4


