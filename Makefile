CROSS   = x86_64-w64-mingw32-
CC      = gcc
CFLAGS  = -Os
LDFLAGS =

pkg-config.exe: win32_main.c cmdline.c u-config.c
	$(CROSS)$(CC) \
	    -fwhole-program -fno-asynchronous-unwind-tables \
	    -s -nostdlib -Wl,--gc-sections -o $@ $(CFLAGS) $(LDFLAGS) \
	    win32_main.c -lkernel32

pkg-config-debug.exe: win32_main.c cmdline.c u-config.c
	$(CROSS)$(CC) -g3 -DDEBUG \
	   -Wall -Wextra -Wconversion -Wno-sign-conversion \
	   -fsanitize=undefined -fsanitize-undefined-trap-on-error \
	   -nostdlib -o $@ win32_main.c -lkernel32

# Configure using the system's pkg-config search path
pkg-config: generic_main.c u-config.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ generic_main.c \
	    -DPKG_CONFIG_LIBDIR="\"$$(pkg-config --variable pc_path pkg-config)\""

pkg-config-debug: generic_main.c u-config.c
	$(CC) -g3 -DDEBUG -Wall -Wextra -Wconversion -Wno-sign-conversion \
	   -fsanitize=address,undefined -o $@ generic_main.c

pkg-config-linux-amd64: linux_amd64_main.c u-config.c
	$(CC) $(CFLAGS) -fno-asynchronous-unwind-tables -fno-pie -fwhole-program \
	   -s -no-pie -nostdlib -Wl,--gc-sections -o $@ linux_amd64_main.c \
	   -DPKG_CONFIG_LIBDIR="\"$$(pkg-config --variable pc_path pkg-config)\""

pkg-config-linux-amd64-debug: linux_amd64_main.c u-config.c
	$(CC) -g3 -DDEBUG \
	   -Wall -Wextra -Wconversion -Wno-sign-conversion \
	   -fsanitize=undefined -fsanitize-undefined-trap-on-error \
	   -nostdlib -o $@ linux_amd64_main.c

amalgamation: pkg-config.c
pkg-config.c: u-config.c cmdline.c win32_main.c
	awk 'n{print"";n=0} NR==3{printf"%s\n%s\n",cc,cl} !/^#i.*"/{print}' \
	    cc='//   $$ cc -nostartfiles -o pkg-config.exe pkg-config.c' \
	    cl='//   $$ cl pkg-config.c' \
	    >$@ u-config.c n=1 cmdline.c n=1 win32_main.c

release:
	version=$$(git describe); prefix=u-config-$${version#v}; \
	  git archive --prefix=$$prefix/ HEAD | gzip -9 >$$prefix.tar.gz

tests.exe: test_main.c u-config.c
	$(CROSS)$(CC) -g3 -o $@ test_main.c \
	    -Wall -Wextra -Wconversion -Wno-sign-conversion \
	    -Wno-clobbered -Wno-for-loop-analysis -Wno-unknown-warning-option \
	    -fsanitize=undefined -fsanitize-undefined-trap-on-error

tests: test_main.c u-config.c
	$(CC) -g3 -o $@ test_main.c \
	    -Wall -Wextra -Wconversion -Wno-sign-conversion \
	    -Wno-clobbered -Wno-for-loop-analysis -Wno-unknown-warning-option \
	    -fsanitize=address,undefined

check: tests$(EXE)
	./tests$(EXE)

clean:
	rm -f pkg-config.exe pkg-config-debug.exe \
	      pkg-config pkg-config-debug \
	      pkg-config-linux-amd64 pkg-config-linux-amd64-debug \
	      pkg-config.c \
	      tests.exe tests
