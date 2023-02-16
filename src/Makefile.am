sbin_PROGRAMS = pkg_chk
pkg_chk_SOURCES = \
	config_file.hxx config_file.cxx \
	makevars.cxx makevars.hxx \
	main.cxx \
	environment.cxx environment.hxx \
	fdstream.hxx fdstream.cxx \
	harness.hxx harness.cxx \
	message.cxx message.hxx \
	nursery.cxx nursery.hxx \
	options.cxx options.hxx \
	ordered.hxx \
	pkgdb.cxx pkgdb.hxx \
	pkgpath.cxx pkgpath.hxx \
	pkgname.cxx pkgname.hxx \
	pkgpattern.cxx pkgpattern.hxx \
	string_algo.hxx \
	summary.hxx summary.cxx \
	tag.cxx tag.hxx \
	todo.hxx todo.cxx \
	xargs_fold.hxx
pkg_chk_CXXFLAGS = \
	$(WARN_CXXFLAGS) \
	-DCFG_BMAKE='"$(BMAKE)"' \
	-DCFG_MAKECONF='"$(MAKECONF)"' \
	-DCFG_PKG_CONFIG='"$(PKG_CONFIG)"' \
	-DCFG_PKG_CONFIG_PATH='"$(PKG_CONFIG_PATH)"' \
	-DCFG_PKG_CONFIG_LIBDIR='"$(PKG_CONFIG_LIBDIR)"' \
	-DCFG_PREFIX='"$(prefix)"' \
	-DCFG_UNAME='"$(UNAME)"' \
	-DCFG_XARGS='"$(XARGS)"'

AM_CFLAGS = $(WARN_CFLAGS)
AM_CXXFLAGS = $(WARN_CXXFLAGS)
AM_LDFLAGS = $(WARN_LDFLAGS)
