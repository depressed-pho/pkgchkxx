sbin_PROGRAMS = pkg_chk
pkg_chk_SOURCES = \
	bzip2stream.cxx bzip2stream.hxx \
	build_version.hxx build_version.cxx \
	config_file.hxx config_file.cxx \
	check.hxx check.cxx \
	makevars.cxx makevars.hxx \
	main.cxx \
	environment.cxx environment.hxx \
	fdstream.hxx fdstream.cxx \
	gzipstream.cxx gzipstream.hxx \
	harness.hxx harness.cxx \
	message.cxx message.hxx \
	mutex_guard.hxx \
	nursery.cxx nursery.hxx \
	options.cxx options.hxx \
	ordered.hxx \
	pkgpath.cxx pkgpath.hxx \
	pkgname.cxx pkgname.hxx \
	pkgpattern.cxx pkgpattern.hxx \
	string_algo.hxx \
	summary.hxx summary.cxx \
	tag.cxx tag.hxx \
	tempfile.cxx tempfile.hxx \
	todo.hxx todo.cxx \
	wwwstream.cxx wwwstream.hxx \
	xargs_fold.hxx
pkg_chk_CXXFLAGS = \
	$(AM_CXXFLAGS) \
	$(BZIP2_CPPFLAGS) \
	$(LIBFETCH_CPPFLAGS) \
	$(ZLIB_CFLAGS) \
	-DCFG_BMAKE='"$(BMAKE)"' \
	-DCFG_MAKECONF='"$(MAKECONF)"' \
	-DCFG_PKG_ADD='"$(PKG_ADD)"' \
	-DCFG_PKG_ADMIN='"$(PKG_ADMIN)"' \
	-DCFG_PKG_CONFIG='"$(PKG_CONFIG)"' \
	-DCFG_PKG_CONFIG_PATH='"$(PKG_CONFIG_PATH)"' \
	-DCFG_PKG_CONFIG_LIBDIR='"$(PKG_CONFIG_LIBDIR)"' \
	-DCFG_PKG_DELETE='"$(PKG_DELETE)"' \
	-DCFG_PKG_INFO='"$(PKG_INFO)"' \
	-DCFG_PREFIX='"$(prefix)"' \
	-DCFG_TSORT='"$(TSORT)"' \
	-DCFG_UNAME='"$(UNAME)"' \
	-DCFG_XARGS='"$(XARGS)"'
pkg_chk_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(BZIP2_LIBS) \
	$(LIBFETCH_LIBS) \
	$(ZLIB_LIBS)

AM_CFLAGS = $(WARN_CFLAGS)
AM_CXXFLAGS = $(WARN_CXXFLAGS)
AM_LDFLAGS = $(WARN_LDFLAGS)
