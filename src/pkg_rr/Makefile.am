sbin_PROGRAMS = pkgrrxx

pkgrrxx_SOURCES = \
	environment.cxx environment.hxx \
	scanner.cxx scanner.hxx \
	main.cxx \
	message.cxx message.hxx \
	options.cxx options.hxx \
	replacer.cxx replacer.hxx

pkgrrxx_CXXFLAGS = \
	-I$(top_builddir)/lib \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/src

pkgrrxx_LDADD = \
	$(top_builddir)/lib/pkgxx/libpkgxx.la \
	$(top_builddir)/src/pkg_chk/libpkgchkxx.la
