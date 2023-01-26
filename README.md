# u-config: a small, simple, portable pkg-config clone

u-config ("*micro*-config") is a tiny implementation of [pkg-config][],
primarily to support [w64devkit][] and Windows. It was born of frustration
with pkg-config and [pkgconf][]. Key features:

* A fraction of the size while retaining the core, user-level features of
  pkg-config and pkgconf.

* Windows as a first-class supported platform.

* Highly portable to any machine. Can be built without libc, which is
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
* Omits clunky redundant features (`--errors-to-stdout`, etc.)
* Less strict `.pc` syntax

It still supports the important pkg-config run-time environment variables:

* `PKG_CONFIG_PATH`
* `PKG_CONFIG_LIBDIR`
* `PKG_CONFIG_TOP_BUILD_DIR`
* `PKG_CONFIG_SYSTEM_INCLUDE_PATH`
* `PKG_CONFIG_SYSTEM_LIBRARY_PATH`

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
layer (`*_main.c`) for your target then invoke your C compiler only that
source file. The "generic" platform is libc so it works everywhere, but
inherits the target's libc limitations (restricted path and environment
variable access, no automatic self-configuration, etc.).

    $ cc -Os -o pkg-config generic_main.c

However, one of core goals is to be a reliable, native pkg-config for
Windows, so it has a dedicated platform layer. This layer understands
Unicode paths and environment variables — though keep in mind that it's
probably interacting with tools that do not. It outputs arguments encoded
in UTF-8 regardless of the system code page. Do not link a C runtime (CRT)
in this configuration.

    $ gcc -Os -nostartfiles -o pkg-config win32_main.c

Or with MSVC (automatically omits a CRT):

    $ cl /O2 /Fe:pkg-config win32_main.c

As a convenience, the Makefile provides a more aggressively optimized GCC
build configuration.

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

### x86-64 Linux configuration options

`linux_amd64_main.c` makes direct Linux system calls using assembly and
does not require libc. It compiles to a ~20kB static executable that will
work on any x86-64 Linux host. It supports these configuration macros with
the same behavior as the generic platform.

* `PKG_CONFIG_LIBDIR`
* `PKG_CONFIG_SYSTEM_INCLUDE_PATH`
* `PKG_CONFIG_SYSTEM_LIBRARY_PATH`

### Debugging

The `DEBUG` macro enables assertions. Suggested debug build, which is
intended to be run under a debugger:

    $ cc -g3 -DDEBUG -Wall -Wextra -Wconversion -Wno-sign-conversion \
         -fsanitize=undefined -fsanitize-undefined-trap-on-error \
         PLATFORM_main.c

For MSVC with maximum run-time error checks (RTC):

    $ cl /Z7 /DDEBUG /W4 /RTCcsu PLATFORM_main.c

RTC and Address Sanitizer (`/fsanitize=address`) are mutually exclusive,
and the former is more useful for u-config.

### Test suite

The test suite is a libc-based platform layer and runs u-config through
its entry point in various configurations on a virtual file system. Either
build and run `test_main.c` as a platform, or use the suggested test
configuration in the Makefile (set `EXE=.exe` on Windows):

    $ make check

For MSVC:

    $ cl /Z7 /W4 /RTCcsu test_main.c

### Fuzz testing

`fuzz_main.c` is a platform layer implemented on top of [AFL++][]. Fuzzer
input is supplied through a virtual file, and it exercises `.pc` parsing,
variable expansion, and output processing.

    $ afl-clang-fast -fsanitize=address,undefined fuzz_main.c
    $ afl-fuzz -m32T -i/usr/share/pkgconfig -ofuzzout ./a.out


[AFL++]: https://github.com/AFLplusplus/AFLplusplus
[pkg-config]: https://www.freedesktop.org/wiki/Software/pkg-config/
[pkgconf]: http://pkgconf.org/
[w64devkit]: https://github.com/skeeto/w64devkit
