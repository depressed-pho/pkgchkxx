SUBDIRS = doc lib src

EXTRA_DIST = \
	HACKING.md \
	NEWS.md \
	README.md

# Let "make distcheck" use variables supplied to ./configure
DISTCHECK_CONFIGURE_FLAGS = \
	CC="${CC}" \
	CXX="${CXX}" \
	CPPFLAGS="${CPPFLAGS}" \
	CFLAGS="${CFLAGS}" \
	CXXFLAGS="${CXXFLAGS}" \
	LDFLAGS="${LDFLAGS}"
