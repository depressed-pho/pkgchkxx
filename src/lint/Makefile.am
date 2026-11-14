bin_PROGRAMS = lintpkgxx

lintpkgxx_SOURCES = \
	environment.cxx environment.hxx \
	lintdb.hxx lintdb.cxx \
	main.cxx \
	options.cxx options.hxx

lintpkgxx_CXXFLAGS = \
	-I$(top_builddir)/lib \
	-I$(top_srcdir)/lib

lintpkgxx_LDADD = \
	$(top_builddir)/lib/pkgxx/libpkgxx.la
