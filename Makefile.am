SUBDIRS = doc lib src

EXTRA_DIST = \
	README.md \
	HACKING.md

# Let "make distcheck" use variables supplied to ./configure
DISTCHECK_CONFIGURE_FLAGS = \
	CC="${CC}" \
	CXX="${CXX}" \
	CPPFLAGS="${CPPFLAGS}" \
	CFLAGS="${CFLAGS}" \
	CXXFLAGS="${CXXFLAGS}" \
	LDFLAGS="${LDFLAGS}"
