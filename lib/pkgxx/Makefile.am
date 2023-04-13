noinst_LTLIBRARIES = libpkgxx.la

libpkgxx_la_SOURCES = \
	build_version.hxx build_version.cxx \
	bzip2stream.cxx bzip2stream.hxx \
	environment.cxx environment.hxx \
	fdstream.hxx fdstream.cxx \
	graph.hxx \
	gzipstream.cxx gzipstream.hxx \
	harness.hxx harness.cxx \
	makevars.cxx makevars.hxx \
	mutex_guard.hxx \
	nursery.cxx nursery.hxx \
	ordered.hxx \
	pkgdb.cxx pkgdb.hxx \
	pkgname.cxx pkgname.hxx \
	pkgpath.cxx pkgpath.hxx \
	pkgpattern.cxx pkgpattern.hxx \
	string_algo.hxx \
	summary.hxx summary.cxx \
	tempfile.cxx tempfile.hxx \
	todo.cxx todo.hxx \
	wwwstream.cxx wwwstream.hxx \
	xargs_fold.hxx

libpkgxx_la_CXXFLAGS = \
	-I$(top_srcdir)/lib \
	-DCFG_PREFIX='"$(prefix)"'
	$(BZIP2_CPPFLAGS) \
	$(LIBFETCH_CPPFLAGS) \
	$(ZLIB_CFLAGS)

libpkgxx_la_LDFLAGS = \
	$(BZIP2_LIBS) \
	$(LIBFETCH_LIBS) \
	$(ZLIB_LIBS)
