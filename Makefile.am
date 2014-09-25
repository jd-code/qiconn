
ACLOCAL_AMFLAGS = -I m4

vimtest: all

## this target is for maintainers
tarfiles.txt:
	repozclean && autoall
	( find . | grep -v '\.git' | grep -v 'debian-src/' ) > tarfiles.txt

tar: tarfiles.txt qiconn.cpp include/qiconn/qiconn.h
	tar -zcphf qiconn_"${VERSION}".orig.tar.gz --no-recursion --transform='s=^='qiconn_"${VERSION}"'/=' -T tarfiles.txt
	tar -ztvf qiconn_"${VERSION}".orig.tar.gz

pkginclude_HEADERS = include/qiconn/qiconn.h

lib_LTLIBRARIES = libqiconn.la
noinst_LIBRARIES = libqiconn.a

libqiconn_la_SOURCES = qiconn.cpp
libqiconn_la_CPPFLAGS = $(AM_CPPFLAGS)
libqiconn_la_LDFLAGS = @LIBQICONN_LDFLAGS@

libqiconn_a_SOURCES = qiconn.cpp
libqiconn_a_CPPFLAGS = $(AM_CPPFLAGS)
libqiconn_a_LDFLAGS = @LIBQICONN_LDFLAGS@

doc: include/qiconn/*.h *.cpp qiconn.dox
	doxygen qiconn.dox

clean-local:
	rm -rf qiconn-doc tarfiles.txt qiconn_"${VERSION}".orig.tar.gz

distclean-local:
	rm -f m4/*.m4


