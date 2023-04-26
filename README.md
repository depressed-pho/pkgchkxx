# What's this

**pkgchkxx** and **pkgrrxx** are complete rewrites of
[pkgsrc](https://www.pkgsrc.org/)
[pkg_chk](https://pkgsrc.se/pkgtools/pkg_chk) and
[pkg_rolling-replace](https://pkgsrc.se/pkgtools/pkg_rolling-replace)
respectively. These are functionally compatible but run faster:

* `pkgrrxx -u` runs 2.96x faster than `pkg_rolling-replace -u`.
* `pkgrrxx -sn` runs 14.8x faster than `pkg_rolling-replace -sn`.
* `pkgchkxx -aur -b` runs 11x faster than `pkg_chk -aur -b` when
  [pkg_summary(5)](https://man.netbsd.org/pkg_summary.5) file is available.
* `pkgchkxx -aur -s` runs 3x faster than `pkg_chk -aur -s`.
* `pkgchkxx -l` runs 185x faster than `pkg_chk -l` when
  `pkg_summary(5)` file is available, and runs 24.8x faster when it's
  unavailable (and needs to scan archives).
* `pkgchkxx -p` runs 1.3x faster than `pkg_chk -p`.
* `pkgchkxx -g` runs 1.6x faster than `pkg_chk -g`.
* `pkgchkxx -N` runs 22x faster than `pkg_chk -N`.

This implementation achieves better performance by using better algorithms
and making use of many CPUs whenever possible. The latter is the primary
reason why a complete rewrite was needed: doing it in POSIX shell was
simply not feasible.


## External dependencies

* A C++17 compiler. For GCC it has to be GCC 8 or later.
* [GNU Make](https://www.gnu.org/software/make/make.html), only needed for
  building the programs.
* [pkg_install](https://pkgsrc.se/pkgtools/pkg_install), obviously.
* [pkg-config](https://pkgconfig.freedesktop.org/) for determining what
  **pkg_chk** tags to predefine. *This is a run-time dependency.*
* [bzip2](https://sourceware.org/bzip2/) for reading bzip2-compressed
  `pkg_summary(5)` files.
* [zlib](https://www.zlib.net/) for reading gzip-compressed
  `pkg_summary(5)` files.
* [libfetch](https://pkgsrc.se/net/libfetch) for fetching
  `pkg_summary(5)` files from a remote host.


## Release notes

See [NEWS](./NEWS.md).


## Building and installation

```
% ./configure
% gmake
% sudo gmake install
```

You may need to give `./configure` the path to your compiler if the
system compiler doesn't support C++17:

```
% ./configure CXX=/path/to/cxx
```


## Hacking

See [HACKING](./HACKING.md).


## License

`BSD-2-Clause` AND `BSD-3-Clause`. See [COPYING](./COPYING).


## Author

[The NetBSD Foundation](http://www.netbsd.org/foundation/)
