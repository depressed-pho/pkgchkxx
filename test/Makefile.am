check_PROGRAMS = \
	bzip2stream \
	gzipstream \
	shell_args
AM_DEFAULT_SOURCE_EXT = .cxx

TEST_LOG_DRIVER = env AM_TAP_AWK='$(AWK)' \
	$(SHELL) $(top_srcdir)/build-aux/tap-driver.sh

TESTS = $(check_PROGRAMS)

bzip2stream_LDADD = $(top_builddir)/lib/pkgxx/libpkgxx.la
gzipstream_LDADD  = $(top_builddir)/lib/pkgxx/libpkgxx.la
shell_args_LDADD  = $(top_builddir)/lib/pkgxx/libpkgxx.la

AM_CXXFLAGS = \
	-I$(top_builddir)/lib \
	-I$(top_srcdir)/lib
