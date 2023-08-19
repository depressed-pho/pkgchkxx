# Release notes

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
