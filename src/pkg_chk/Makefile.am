sbin_PROGRAMS = pkgchkxx
noinst_LTLIBRARIES = libpkgchkxx.la

#
# pkgchkxx
#
pkgchkxx_SOURCES = \
	config_file.hxx config_file.cxx \
	main.cxx \
	environment.cxx environment.hxx \
	message.cxx message.hxx \
	options.cxx options.hxx \
	tag.cxx tag.hxx

pkgchkxx_CXXFLAGS = \
	-I$(top_builddir)/lib \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/src

pkgchkxx_LDADD = \
	libpkgchkxx.la \
	$(top_builddir)/lib/pkgxx/libpkgxx.la

#
# libpkgchkxx.la
#
libpkgchkxx_la_SOURCES = \
	check.hxx check.cxx

libpkgchkxx_la_CXXFLAGS = \
	-I$(top_builddir)/lib \
	-I$(top_srcdir)/lib

libpkgchkxx_la_LIBADD = \
	$(top_builddir)/lib/pkgxx/libpkgxx.la
