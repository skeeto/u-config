# u-config: a small, simple, portable pkg-config clone

u-config ("*micro*-config") is a small, highly portable [pkg-config][] and
[pkgconf][] clone. It was written primarily for [w64devkit][] and Windows,
but can also serve as a reliable drop-in replacement on other platforms.
Notable features:

* A fraction of the size while retaining the core, user-level features of
  pkg-config and pkgconf.

* Windows as a first-class supported platform.

* Highly portable to any machine. Supports a variety of compilers and
  operating systems, ancient and new. Can be built without libc, which is
  handy for bootstrapping.

* Trivial, fast build. No messing around with GNU Autotools, or any build
  system for that matter.

Each pkg-config is specialized to its surrounding software distribution,
and so **u-config is probably only useful to distribution maintainers**.

As noted, u-config focuses on *user-level* features but deliberately lacks
*developer-level* features. The goal is to **support existing pkg-config
based builds, not make more of them**. In summary:

* Omits most `.pc` debugging features (`--print-…`)
* No special handling of "uninstalled" packages, and no attendant knobs
* Skips checks unimplemented by pkg-config (i.e. `Conflicts:`)
* Less strict `.pc` syntax

It still supports the important pkg-config run-time environment variables:

* `PKG_CONFIG_PATH`
* `PKG_CONFIG_LIBDIR`
* `PKG_CONFIG_TOP_BUILD_DIR`
* `PKG_CONFIG_SYSTEM_INCLUDE_PATH`
* `PKG_CONFIG_SYSTEM_LIBRARY_PATH`
* `PKG_CONFIG_ALLOW_SYSTEM_CFLAGS`
* `PKG_CONFIG_ALLOW_SYSTEM_LIBS`

See the pkg-config documentation for details. It also closely follows
pkg-config's idiosyncratic argument parsing — positional arguments are
concatenated then retokenzied — and its undocumented `.pc` quote and
backslash syntax.

## Features unique to u-config

* `--newlines`: separates arguments by line feeds instead of spaces, which
  is sometimes useful like in the fish shell or when manually examining
  output.

* Handles spaces in `prefix`: Especially important on Windows where spaces
  in paths are common. Libraries will work correctly even when installed
  under such a path. (Note: Despite popular belief, and the examples in
  its own documentation, *pkg-config was never designed for use in command
  substitution*. It was designed for `eval`, and spaces in `prefix` will
  require implicit or explicit `eval`.)

## Build

u-config compiles as one translation unit. Choose an appropriate platform
layer (`main_*.c`) for your target then invoke your C compiler only that
source file. The unadorned `main.c` is libc so it works everywhere, but
inherits the target's libc limitations (restricted path and environment
variable access, no automatic self-configuration, etc.).

    $ cc -Os -o pkg-config main.c

However, one of core goals is to be a reliable, native pkg-config for
Windows, so it has a dedicated platform layer. This layer understands
Unicode paths and environment variables — though keep in mind that it's
probably interacting with tools that do not. It outputs arguments encoded
in UTF-8 regardless of the system code page. Do not link a C runtime (CRT)
in this configuration.

    $ cc -Os -nostartfiles -o pkg-config main_windows.c

Or with MSVC:

    $ cl /GS- /O2 /Os /Fe:pkg-config main_msvc.c

The Makefile documents compiler options for a more aggressively optimized
GCC-based build.

## Configuration

The default search path tries to be useful, but the `pkgconfig` directory
locations are unpredictable and largely distribution-specific. Once known,
use the macros below to hard code the correct paths into the binary. To
double check your configuration, examine `pc_path`:

    $ ./pkg-config --variable pc_path pkg-config

The `pkg-config` package is virtual and need not exist as an actual `.pc`
file installed in the system. The `pkg-config` make target automatically
grabs the search path from the system `pkg-config`, and so could then be a
drop-in replacement for it.

    $ make pkg-config   # grabs system's pkg-config search path

### Generic configuration options

The "generic" platform has several compile time configuration parameters.
Each must be formatted as a C string with quotes. Relative paths will be
relative to the run-time working directory, not the installation prefix,
which is usually not useful.

* `PKG_CONFIG_PATH`: Like the run-time environment variable, but places
  these additional paths ahead of the standard search locations. If the
  standard search locations are insufficient, this is probably what you
  want.

* `PKG_CONFIG_PREFIX` (default `"/usr"`): The default prefix for the four
  standard search locations.

* `PKG_CONFIG_LIBDIR`: Like the run-time environment variable, but fully
  configures the fixed, static search path. If set, `PKG_CONFIG_PREFIX` is
  not used. If the environment variable is set at run time, it replaces
  this path.

* `PKG_CONFIG_DEFINE_PREFIX`: Sets the default for `--define-prefix` /
  `--dont-define-prefix`. Defaults to true on Windows, false otherwise.
  You probably want the default.

* `PKG_CONFIG_SYSTEM_INCLUDE_PATH`: Like the run-time environment variable
  but sets the static default.

* `PKG_CONFIG_SYSTEM_LIBRARY_PATH`: Like the run-time environment variable
  but sets the static default.

Examples:

    $ cc -DPKG_CONFIG_PREFIX="\"$HOME/.local\"" ...
    $ cc -DPKG_CONFIG_PATH="\"$HOME/.local/lib/pkgconfig\"" ...

### Windows configuration options

The "win32" platform always searches relative to `pkg-config.exe` since
it's essentially the only useful behavior. It is rare for anything aside
from system components to be at compile-time-known absolute paths. The
default configuration assumes pkg-config resides in `bin/` adjacent to
`lib/` and `share/` — a typical sysroot.

* `PKG_CONFIG_PREFIX`: A relative component placed before the "standard"
  search paths (`lib/pkgconfig`, `share/pkgconfig`). Either slash or
  backslash may be used to separate path components, but slash reduces
  problems.

Example, if just outside a sysroot which identifies the architecture:

    $ gcc -DPKG_CONFIG_PREFIX=\"/$(gcc -dumpmachine)\" ...

While both are supported, u-config prefers slashes over backslashes in
order to reduce issues involving backslash as a shell metacharacter.

### libc-free Linux configuration options

`main_linux_*.c` makes direct Linux system calls using assembly and does
not require libc. It compiles to a ~20kB static executable that will work
on any Linux distribution. It supports these configuration macros with the
same behavior as the generic platform.

* `PKG_CONFIG_LIBDIR`
* `PKG_CONFIG_SYSTEM_INCLUDE_PATH`
* `PKG_CONFIG_SYSTEM_LIBRARY_PATH`

### Debugging

Suggested debug build, intended to be run under a debugger:

    $ cc -g3 -Wall -Wextra -Wconversion -Wno-sign-conversion \
         -fsanitize=undefined -fsanitize-trap main_PLATFORM.c

Enabling Undefined Behavior Sanitizer also enables assertions.

### Test suite

The test suite is a libc-based platform layer and runs u-config through
its entry point in various configurations on a virtual file system. Either
build and run `main_test.c` as a platform, or use the suggested test
configuration in the Makefile (set `EXE=.exe` on Windows):

    $ make check

### Fuzz testing

`main_fuzz.c` is a platform layer implemented on top of [AFL++][]. Fuzzer
input is supplied through a virtual file, and it exercises `.pc` parsing,
variable expansion, and output processing. Its header documents suggested
usage.


[AFL++]: https://github.com/AFLplusplus/AFLplusplus
[pkg-config]: https://www.freedesktop.org/wiki/Software/pkg-config/
[pkgconf]: http://pkgconf.org/
[w64devkit]: https://github.com/skeeto/w64devkit
