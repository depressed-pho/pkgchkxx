# Release notes

## 0.3.1 -- 2025-02-10

* Fix an issue where `pkgrrxx` (and possibly `pkgchkxx` also) would die
  when it receives a signal while it's blocking on `waitpid(2)`.
* Fix an issue in 0.3 where ANSI escape sequences would leak into `make`
  output.

## 0.3 -- 2025-02-10

* `pkgrrxx` now uses colours in its output if `stderr` is a tty and the
  environment variable `NO_COLOR` is not defined.

## 0.2.5 -- 2025-01-26

* Fixed build on Illumos.
* Fixed a stability issue on systems that lack `posix_spawn(3)`. It could
  sometimes deadlock because of `malloc(3)` after `fork(2)`.

## 0.2.4 -- 2025-01-18

* `pkgrrxx -u` now displays a progress bar while checking for outdated
  packages, if `stderr` is a tty.
* Fix an issue where `pkgchkxx -u` can access invalidated memory and then
  crash. The bug was introduced at version 0.2.2. Reported by @ryoon [#9]
  and @0323pin [#10].
* Fix the formatting of `pkgchkxx` displaying commands to run. There was a
  space missing between time and command.
* Fix `pkgchkxx -un` not simulating the effect of `pkg_delete -r`. The same
  issue exists in the original `pkg_chk` but it'd be nice to fix it.

## 0.2.3 -- 2025-01-16

* Gave up on the `fast-clean` option. `pkgrrxx` previously attempted to
  simulate what `make clean` does instead of just running it, because
  running `make` is slow. But it turned out to be a losing battle due to
  subtlety around `${WRKOBJDIR}` so we just run `make clean` now. Issue
  reported by @schmonz [#6]

## 0.2.2 -- 2025-01-16

* Fix an issue where `pkgchkxx -u -q` scans and prints outdated packages
  twice, reported by @pfr-dev [#5]
* Fix an issue where `pkgchkxx -u` deletes outdated packages but then fails
  to install their newer versions. This was broken from the beginning of
  `pkgchkxx` and it's a good thing that nobody bothered to use this mode
  (as opposed to `pkgrrxx`).
* Fix a build failure on platforms where `posix_spawn(3)` is unavailable,
  and either `execvpe(3)` or `execve(2)` is also missing, reported by
  @schmonz [#7].
* Fix a potential issue where encountering an error condition could make
  programs die with SIGABRT instead of exitting gracefully.

## 0.2.1 -- 2024-05-26

* Fix compilation on Linux (Fedora 40), patch by @bsiegert [#3]
* Fix a bug in 0.2 where defining `${WRKOBJDIR}` in `mk.conf` makes
  `pkgrrxx` fail to clean `WRKDIR`, reported by @schmonz [#4]

## 0.2 -- 2024-04-22

* Performance improvement: `pkgchkxx -s` no longer invokes `make update
  CLEANDEPENDS=yes` but now uses `make update
  DEPENDS_TARGET='package-install clean'`.
* Performance improvement: `pkgrrxx` by default now removes working
  directories directly instead of running `make clean`, which is slow. You
  can disable this optimization with a configure option
  `--disable-fast-clean`.
* Performance improvement: Tools now use `posix_spawn(3)` on platforms
  where it exists, and fall back to `fork` & `exec` where it doesn't.
* `pkgrrxx` now shows the number of entries in each non-empty TODO list, to
  give the user some clue about the time it's going to take.
* Fixed an issue where `RR> ` could be printed twice depending on how the
  C++ compiler optimized the code.

## 0.1.6 -- 2023-08-19

* Fix a bug in `pkgrrxx` where invoking `pkg_admin` would always fail. The
  bug was introduced in 0.1.5.

## 0.1.5 -- 2023-08-06

* Work around an issue where `FETCH_USING` is listed in `BOOTSTRAP_DEPENDS`
  and creates a dependency cycle, reported by Oskar.

## 0.1.4 -- 2023-08-05

* Fix a bug in dependency cycle detection. Previously it didn't only failed
  to display a correct cycle but also derefenced an invalid pointer,
  leading to a garbled output and possibly even a crash.

## 0.1.3 -- 2023-08-02

* Fix a segfault occuring when an external command exits with a non-zero
  status, reported by Oskar.
* Fix a bug in `pkgrrxx` where deinstalling build-only dependencies of
  packages confuses the tool, reported by Oskar.
* Fix a bug in `pkgrrxx -n` where dry-runs could hang up in the
  `Re-checking for unsafe installed package` phase.
* Fix a bug in `pkgrrxx -n` where dry-runs attempt to set `automatic=YES`
  on packages it doesn't actually install.
* Fix a bug in `pkgrrxx -n` where dry-runs attempt to find packages that
  depend on a package that isn't installed and fail.

## 0.1.2 -- 2023-07-31

* Fix a bunch of compiler warnings (#2)
* Clarify that pkgrrxx doesn't spawn pkg_chk internally.

## 0.1.1 -- 2023-07-23

* Fix macOS build (#1)

## 0.1 -- 2023-07-22

* Initial release.
