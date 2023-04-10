sbin_PROGRAMS = pkgrrxx

pkgrrxx_SOURCES = \
	main.cxx \
	options.cxx options.hxx

pkgrrxx_CXXFLAGS = \
	-I$(top_srcdir)/lib \
	-DCFG_PREFIX='"$(prefix)"'

pkgrrxx_LDADD = \
	$(top_builddir)/lib/pkgxx/libpkgxx.la
