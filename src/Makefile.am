sbin_PROGRAMS = pkg_chk
pkg_chk_SOURCES = \
	makevars.cxx makevars.hxx \
	main.cxx \
	environment.cxx environment.hxx \
	fdstream.hxx fdstream.cxx \
	harness.hxx harness.cxx \
	message.cxx message.hxx \
	nursery.cxx nursery.hxx \
	options.cxx options.hxx \
	pkgname.cxx pkgname.hxx \
	tag.hxx \
	todo.hxx todo.cxx
pkg_chk_CXXFLAGS = \
	$(WARN_CXXFLAGS) \
	-DCFG_BMAKE='"$(BMAKE)"' \
	-DCFG_MAKECONF='"$(MAKECONF)"' \
	-DCFG_PREFIX='"$(prefix)"'

AM_CFLAGS = $(WARN_CFLAGS)
AM_CXXFLAGS = $(WARN_CXXFLAGS)
AM_LDFLAGS = $(WARN_LDFLAGS)
