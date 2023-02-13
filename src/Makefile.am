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
	pkgdb.cxx pkgdb.hxx \
	pkgname.cxx pkgname.hxx \
	string_algo.hxx \
	tag.cxx tag.hxx \
	todo.hxx todo.cxx
pkg_chk_CXXFLAGS = \
	$(WARN_CXXFLAGS) \
	-DCFG_BMAKE='"$(BMAKE)"' \
	-DCFG_MAKECONF='"$(MAKECONF)"' \
	-DCFG_PKG_CONFIG='"$(PKG_CONFIG)"' \
	-DCFG_PKG_CONFIG_PATH='"$(PKG_CONFIG_PATH)"' \
	-DCFG_PKG_CONFIG_LIBDIR='"$(PKG_CONFIG_LIBDIR)"' \
	-DCFG_PREFIX='"$(prefix)"' \
	-DCFG_UNAME='"$(UNAME)"'

AM_CFLAGS = $(WARN_CFLAGS)
AM_CXXFLAGS = $(WARN_CXXFLAGS)
AM_LDFLAGS = $(WARN_LDFLAGS)
