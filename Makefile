CROSS = x86_64-w64-mingw32-
CC    = gcc

pkg-config.exe: win32_main.c u-config.c
	$(CROSS)$(CC) -Os -fwhole-program -fno-asynchronous-unwind-tables \
	      -s -nostdlib -Wl,--gc-sections -o $@ \
	      win32_main.c -lkernel32 -lshell32

# Configure using the system's pkg-config search path
pkg-config: generic_main.c u-config.c
	$(CC) -g3 -DDEBUG -fsanitize=undefined,address -o $@ generic_main.c \
	  -DPKG_CONFIG_LIBDIR="\"$$(pkg-config --variable pc_path pkg-config)\""

amalgamation: pkg-config.c
pkg-config.c: u-config.c win32_main.c
	cat u-config.c win32_main.c | sed '/^#include "/d' >$@

debug.exe: win32_main.c u-config.c
	$(CROSS)$(CC) -g3 -DDEBUG -Wall -Wextra -Wconversion -Wno-sign-conversion \
	   -fsanitize=undefined -fsanitize-undefined-trap-on-error \
	   -nostdlib -o $@ win32_main.c -lkernel32 -lshell32

debug: generic_main.c u-config.c
	$(CC) -g3 -DDEBUG -Wall -Wextra -Wconversion -Wno-sign-conversion \
	   -fsanitize=undefined -fsanitize-undefined-trap-on-error \
	   -o $@ generic_main.c

clean:
	rm -f pkg-config.exe pkg-config.c debug.exe debug
