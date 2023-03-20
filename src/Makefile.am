sbin_PROGRAMS = pkg_chk

pkg_chk_SOURCES = \
	config_file.hxx config_file.cxx \
	check.hxx check.cxx \
	main.cxx \
	environment.cxx environment.hxx \
	message.cxx message.hxx \
	options.cxx options.hxx \
	tag.cxx tag.hxx

pkg_chk_CXXFLAGS = \
	-I$(top_srcdir)/lib \
	-DCFG_PREFIX='"$(prefix)"'

pkg_chk_LDADD = \
	$(top_builddir)/lib/pkgxx/libpkgxx.la
