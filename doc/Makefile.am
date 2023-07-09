#
# Man pages.
#
man_MANS   = pkgchkxx.8 pkgrrxx.8
CLEANFILES = $(man_MANS)
EXTRA_DIST = $(man_MANS:=.in)

$(man_MANS): %: %.in Makefile
	$(SED) < $< > $@ \
		-e 's|[@]MAKECONF@|$(sysconfdir)/mk.conf|g' \
		-e 's|[@]PREFIX@|$(prefix)|g' \
		-e 's|[@]PKGCHKXX@|'`echo pkgchkxx | sed '@program_transform_name@'`'|g' \
		-e 's|[@]PKGCHKXX_uc@|'`echo pkgchkxx | sed '@program_transform_name@' | tr a-z A-Z`'|g' \
		-e 's|[@]PKGRRXX@|'`echo pkgrrxx | sed '@program_transform_name@'`'|g' \
		-e 's|[@]PKGRRXX_uc@|'`echo pkgrrxx | sed '@program_transform_name@' | tr a-z A-Z`'|g'

#
# API documentation, generated with Doxygen.
#
if ENABLE_DOXYGEN
Doxyfile: Doxyfile.in Makefile

CLEANFILES+= stamp-doxygen
all-local: stamp-doxygen
stamp-doxygen: Doxyfile $(top_srcdir)/*/*/*.?xx
	$(DOXYGEN) Doxyfile
	touch $@

install-data-local: install-api-doc
.PHONY: install-api-doc
install-api-doc: stamp-doxygen
	mkdir -p "$(DESTDIR)$(docdir)"
	(cd _output && tar -cf - .) | (cd "$(DESTDIR)$(docdir)" && tar -xf -)

uninstall-local: uninstall-api-doc
.PHONY: uninstall-api-doc
uninstall-api-doc:
	-rm -rf "$(DESTDIR)$(docdir)"

clean-local: clean-api-doc
.PHONY: clean-api-doc
clean-api-doc:
	-rm -rf _output
endif
