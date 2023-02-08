sbin_PROGRAMS = pkg_chk
pkg_chk_SOURCES = \
	makevars.c makevars.h \
	main.c \
	env.c env.h \
	msg.c msg.h \
	nursery.c nursery.h \
	options.c options.h
pkg_chk_CFLAGS = \
	$(PTHREAD_CFLAGS) \
	-DCFG_BMAKE='"$(BMAKE)"' \
	-DCFG_MAKECONF='"$(MAKECONF)"' \
	-DCFG_PREFIX='"$(prefix)"'
pkg_chk_LIBS = \
	$(PTHREAD_LIBS)

AM_CFLAGS = -std=c11 -Wall
