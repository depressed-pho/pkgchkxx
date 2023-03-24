if ENABLE_DOXYGEN

Doxyfile: Doxyfile.in Makefile
all-local: Doxyfile
	$(DOXYGEN) Doxyfile

install-data-local: stamp-doxygen
	mkdir -p "$(DESTDIR)$(docdir)"
	(cd _output && tar -cf - .) | (cd "$(DESTDIR)$(docdir)" && tar -xf -)

clean-local:
	-rm -rf _output

endif
