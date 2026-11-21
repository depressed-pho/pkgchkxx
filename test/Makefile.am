check_PROGRAMS = shell_args
AM_DEFAULT_SOURCE_EXT = .cxx

TEST_LOG_DRIVER = env AM_TAP_AWK='$(AWK)' \
	$(SHELL) $(top_srcdir)/build-aux/tap-driver.sh

TESTS = $(check_PROGRAMS)

shell_args_LDADD = \
	$(top_builddir)/lib/pkgxx/libpkgxx.la

AM_LDFLAGS = \
	$(ATF_LIBS)

AM_CXXFLAGS = \
	-I$(top_builddir)/lib \
	-I$(top_srcdir)/lib \
	$(ATF_CFLAGS)
