CROSS   = x86_64-w64-mingw32-
CC      = gcc
CFLAGS  = -Os
LDFLAGS =

pkg-config.exe: win32_main.c cmdline.c u-config.c
	$(CROSS)$(CC) \
	    -fwhole-program -fno-builtin -fno-asynchronous-unwind-tables \
	    -s -nostdlib -Wl,--gc-sections -o $@ $(CFLAGS) $(LDFLAGS) \
	    win32_main.c -lkernel32

# Configure using the system's pkg-config search path
pkg-config: generic_main.c u-config.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ generic_main.c \
	  -DPKG_CONFIG_LIBDIR="\"$$(pkg-config --variable pc_path pkg-config)\""

amalgamation: pkg-config.c
pkg-config.c: u-config.c cmdline.c win32_main.c
	sed >$@ '/^#include "/d' u-config.c cmdline.c win32_main.c

debug.exe: win32_main.c cmdline.c u-config.c
	$(CROSS)$(CC) -g3 -DDEBUG -Wall -Wextra -Wconversion -Wno-sign-conversion \
	   -fno-builtin -fsanitize=undefined -fsanitize-undefined-trap-on-error \
	   -nostdlib -o $@ win32_main.c -lkernel32

debug: generic_main.c u-config.c
	$(CC) -g3 -DDEBUG -Wall -Wextra -Wconversion -Wno-sign-conversion \
	   -fsanitize=address,undefined -o $@ generic_main.c

clean:
	rm -f pkg-config.exe pkg-config pkg-config.c debug.exe debug
