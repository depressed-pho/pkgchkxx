sbin_PROGRAMS = pkgchkxx

pkgchkxx_SOURCES = \
	config_file.hxx config_file.cxx \
	check.hxx check.cxx \
	main.cxx \
	environment.cxx environment.hxx \
	message.cxx message.hxx \
	options.cxx options.hxx \
	tag.cxx tag.hxx

pkgchkxx_CXXFLAGS = \
	-I$(top_srcdir)/lib \
	-DCFG_PREFIX='"$(prefix)"'

pkgchkxx_LDADD = \
	$(top_builddir)/lib/pkgxx/libpkgxx.la
