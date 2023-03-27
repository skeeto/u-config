CROSS = x86_64-w64-mingw32-
CC    = gcc
OPT   = -Os

DEBUG_CFLAGS = -g3 -DDEBUG -Wall -Wextra -Wconversion -Wno-sign-conversion \
  -fsanitize=undefined -fsanitize-undefined-trap-on-error
WIN32_CFLAGS = -fwhole-program -fno-asynchronous-unwind-tables
WIN32_LIBS   = -s -nostdlib -Wl,--gc-sections -lkernel32
LINUX_CFLAGS = -fno-asynchronous-unwind-tables -fno-pie -fwhole-program
LINUX_LIBS   = -s -no-pie -nostdlib -Wl,--gc-sections

pkg-config.exe: win32_main.c cmdline.c miniwin32.h u-config.c
	$(CROSS)$(CC) $(OPT) $(WIN32_CFLAGS) -o $@ win32_main.c $(WIN32_LIBS)

pkg-config-debug.exe: win32_main.c cmdline.c miniwin32.h u-config.c
	$(CROSS)$(CC) -nostartfiles $(DEBUG_CFLAGS) -o $@ win32_main.c

# Auto-configure using the system's pkg-config search path
pkg-config: generic_main.c u-config.c
	$(CC) $(OPT) -o $@ generic_main.c \
	  -DPKG_CONFIG_LIBDIR="\"$$(pkg-config --variable pc_path pkg-config)\""

pkg-config-debug: generic_main.c u-config.c
	$(CC) $(DEBUG_CFLAGS) -o $@ generic_main.c

# Auto-configure using the system's pkg-config search path
pkg-config-linux-amd64: linux_amd64_main.c u-config.c
	$(CC) $(OPT) $(LINUX_CFLAGS) -o $@ linux_amd64_main.c $(LINUX_LIBS) \
	  -DPKG_CONFIG_LIBDIR="\"$$(pkg-config --variable pc_path pkg-config)\""

pkg-config-linux-amd64-debug: linux_amd64_main.c u-config.c
	$(CC) -nostdlib $(DEBUG_CFLAGS) -o $@ linux_amd64_main.c

# Concatenate Windows-only u-config into a single source file
amalgamation: pkg-config.c
pkg-config.c: u-config.c cmdline.c miniwin32.h win32_main.c
	awk 'n{print"";n=0} NR==3{printf"%s\n%s\n",cc,cl} !/^#i.*"/{print}' \
	    cc='//   $$ cc -nostartfiles -o pkg-config.exe pkg-config.c' \
	    cl='//   $$ cl pkg-config.c' \
	    >$@ u-config.c n=1 cmdline.c n=1 miniwin32.h n=1 win32_main.c

release:
	version=$$(git describe); prefix=u-config-$${version#v}; \
	  git archive --prefix=$$prefix/ HEAD | gzip -9 >$$prefix.tar.gz

tests.exe: test_main.c u-config.c
	$(CROSS)$(CC) $(DEBUG_CFLAGS) -Wno-clobbered -o $@ test_main.c

tests: test_main.c u-config.c
	$(CC) $(DEBUG_CFLAGS) -Wno-clobbered -o $@ test_main.c

check test: tests$(EXE)
	./tests$(EXE)

# Build and install into w64devkit
install: win32_main.c cmdline.c miniwin32.h u-config.c
	$(CROSS)$(CC) $(OPT) $(WIN32_CFLAGS) win32_main.c \
	  -DPKG_CONFIG_PREFIX="\"/$$(gcc -dumpmachine)\"" \
	  -o $(W64DEVKIT_HOME)/bin/pkg-config.exe $(WIN32_LIBS)

clean:
	rm -f pkg-config.exe pkg-config-debug.exe \
	      pkg-config pkg-config-debug \
	      pkg-config-linux-amd64 pkg-config-linux-amd64-debug \
	      pkg-config.c u-config-*.tar.gz \
	      tests.exe tests \
	      *.ilk *.obj *.pdb test_main.exe
