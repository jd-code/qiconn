
ACLOCAL_AMFLAGS = -I m4

vimtest: all

lib_LTLIBRARIES = libqiconn.la

libqiconn_la_SOURCES = qiconn.cpp

doc: include/qiconn/*.h *.cpp qiconn.dox
	doxygen qiconn.dox

clean-local:
	rm -rf qiconn-doc

distclean-local:
	rm m4/*.m4


