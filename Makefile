# NOTE TO DISTRIBUTION PACKAGE MAINTAINERS: The targets in this Makefile
# are intended only for development, testing, and evaluation. If you are
# packaging u-config for a distribution, invoke a compiler directly on
# the appropriate main_*.c source. See "Build" in README.md for more
# information, and "Configuration" for a list of configuration options.

CROSS = x86_64-w64-mingw32-
CC    = gcc
OPT   = -Os
PC    = pkg-config  # e.g. for CROSS-pkg-config

DEBUG_CFLAGS = -g3 -Wall -Wextra -Wconversion \
  -fsanitize=undefined -fsanitize-undefined-trap-on-error
WIN32_CFLAGS = -fno-builtin -fno-asynchronous-unwind-tables
WIN32_LIBS   = -s -nostdlib -Wl,--gc-sections -lkernel32
LINUX_CFLAGS = -fno-builtin -fno-pie -fno-asynchronous-unwind-tables
LINUX_LIBS   = -static -s -no-pie -nostdlib -Wl,--gc-sections

default:
	@echo 'No sensible default target. Makefile is intended for development'
	@echo 'and testing. See "Build" and "Configuration" in README.md. Use the'
	@echo '"pkg-config" target to copy your system pkg-config configuration'
	@echo 'for evaluation and comparison.'
	@exit 1

src_windows = src/cmdline.c src/miniwin32.h src/u-config.c
src_linux   = src/linux_noarch.c src/memory.c src/u-config.c

pkg-config.exe: main_windows.c $(src_windows)
	$(CROSS)$(CC) $(OPT) $(WIN32_CFLAGS) -o $@ main_windows.c $(WIN32_LIBS)

pkg-config-debug.exe: main_windows.c $(src_windows)
	$(CROSS)$(CC) -nostartfiles $(DEBUG_CFLAGS) -o $@ main_windows.c

# Auto-configure using the system's pkg-config search path
pkg-config: main_posix.c src/u-config.c
	$(CC) $(OPT) -o $@ main_posix.c \
	  -DPKG_CONFIG_LIBDIR="\"$$($(PC) --variable pc_path pkg-config)\""

pkg-config-debug: main_posix.c src/u-config.c
	$(CC) $(DEBUG_CFLAGS) -o $@ main_posix.c

# Auto-configure using the system's pkg-config search path
pkg-config-linux-amd64: main_linux_amd64.c $(src_linux)
	$(CC) $(OPT) $(LINUX_CFLAGS) -o $@ main_linux_amd64.c $(LINUX_LIBS) \
	  -DPKG_CONFIG_LIBDIR="\"$$($(PC) --variable pc_path pkg-config)\""

pkg-config-linux-amd64-debug: main_linux_amd64.c $(src_linux)
	$(CC) -nostdlib -fno-builtin $(DEBUG_CFLAGS) -o $@ main_linux_amd64.c

# Auto-configure using the system's pkg-config search path
pkg-config-linux-i686: main_linux_i686.c $(src_linux)
	$(CC) $(OPT) $(LINUX_CFLAGS) -o $@ main_linux_i686.c $(LINUX_LIBS) \
	  -DPKG_CONFIG_LIBDIR="\"$$($(PC) --variable pc_path pkg-config)\""

pkg-config-linux-i686-debug: main_linux_i686.c $(src_linux)
	$(CC) -nostdlib -fno-builtin $(DEBUG_CFLAGS) -o $@ main_linux_i686.c

# Auto-configure using the system's pkg-config search path
pkg-config-linux-aarch64: main_linux_aarch64.c $(src_linux)
	$(CC) $(OPT) $(LINUX_CFLAGS) -o $@ main_linux_aarch64.c $(LINUX_LIBS) \
	  -DPKG_CONFIG_LIBDIR="\"$$($(PC) --variable pc_path pkg-config)\""

pkg-config-linux-aarch64-debug: main_linux_aarch64.c $(src_linux)
	$(CC) -nostdlib -fno-builtin $(DEBUG_CFLAGS) -o $@ main_linux_aarch64.c

# Concatenate Windows-only u-config into a single source file
amalgamation: pkg-config.c
pkg-config.c: main_windows.c $(src_windows)
	awk 'n{print"";n=0} NR==3{printf"%s\n",cc} !/^#i.*"/{print}' \
	    cc='//   $$ cc -nostartfiles -o pkg-config.exe pkg-config.c' \
	    >$@ src/u-config.c  n=1 \
	        src/miniwin32.h n=1 \
	        src/cmdline.c   n=1 \
	        main_windows.c

release:
	version=$$(git describe); prefix=u-config-$${version#v}; \
	  git archive --prefix=$$prefix/ HEAD | gzip -9 >$$prefix.tar.gz

tests.exe: main_test.c src/u-config.c
	$(CROSS)$(CC) $(DEBUG_CFLAGS) -Wno-clobbered -o $@ main_test.c

tests: main_test.c src/u-config.c
	$(CC) $(DEBUG_CFLAGS) -Wno-clobbered -o $@ main_test.c

pkg-config.wasm: main_wasm.c src/u-config.c
	clang --target=wasm32 -nostdlib -Os -mbulk-memory \
	      -Wall -Wextra -Wconversion -Wno-unused-parameter \
	      -s -Wl,--stack-first -o $@ main_wasm.c

check test: tests$(EXE)
	./tests$(EXE)

# Build and install into w64devkit
install: main_windows.c $(src_windows)
	$(CROSS)$(CC) $(OPT) $(WIN32_CFLAGS) main_windows.c \
	  -DPKG_CONFIG_PREFIX="\"/$$(gcc -dumpmachine)\"" \
	  -o $(W64DEVKIT_HOME)/bin/pkg-config.exe $(WIN32_LIBS)

clean:
	rm -f pkg-config.exe pkg-config-debug.exe \
	      pkg-config pkg-config-debug \
	      pkg-config-linux-amd64 pkg-config-linux-amd64-debug \
	      pkg-config-linux-i686 pkg-config-linux-i686-debug \
	      pkg-config-linux-aarch64 pkg-config-linux-aarch64-debug \
	      pkg-config.c u-config-*.tar.gz \
	      tests.exe tests pkg-config.wasm \
	      *.ilk *.obj *.pdb main_test.exe
