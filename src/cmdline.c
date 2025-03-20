#define CMDLINE_CMD_MAX  32767  // max command line length on Windows
#define CMDLINE_ARGV_MAX (16384+(98298+(i32)sizeof(u8 *))/(i32)sizeof(u8 *))

// Convert an ill-formed-UTF-16 command line to a WTF-8 argv following
// field splitting semantics identical to CommandLineToArgvW, including
// undocumented behavior. Populates argv with pointers into itself and
// returns argc, which is always positive.
//
// Expects that cmd has no more than 32,767 (CMDLINE_CMD_MAX) elements
// including the null terminator, and argv has at least CMDLINE_ARGV_MAX
// elements. This covers the worst possible cases for a Windows command
// string, so no further allocation is ever necessary.
//
// Unlike CommandLineToArgvW, when the command line string is zero
// length this function does not invent an artificial argv[0] based on
// the calling module file name. To implement this behavior yourself,
// test if cmd[0] is zero and then act accordingly.
//
// If the input is UTF-16, then the output is UTF-8.
static i32 cmdline_to_argv8(c16 *cmd, u8 **argv)
{
    i32 argc  = 1;  // worst case: argv[0] is an empty string
    i32 state = 6;  // special argv[0] state
    i32 slash = 0;
    // Use second half as byte buffer
    u8 *buf = (u8 *)(argv + 16384);

    argv[0] = buf;
    while (*cmd) {
        i32 c = *cmd++;
        if (c>>10 == 0x36 && *cmd>>10 == 0x37) {  // surrogates?
            c = 0x10000 + ((c - 0xd800)<<10) + (*cmd++ - 0xdc00);
        }

        switch (state) {
        case 0: switch (c) {  // outside token
                case 0x09:
                case 0x20: continue;
                case 0x22: argv[argc++] = buf;
                           state = 2;
                           continue;
                case 0x5c: argv[argc++] = buf;
                           slash = 1;
                           state = 3;
                           break;
                default  : argv[argc++] = buf;
                           state = 1;
                } break;
        case 1: switch (c) {  // inside unquoted token
                case 0x09:
                case 0x20: *buf++ = 0;
                           state = 0;
                           continue;
                case 0x22: state = 2;
                           continue;
                case 0x5c: slash = 1;
                           state = 3;
                           break;
                } break;
        case 2: switch (c) {  // inside quoted token
                case 0x22: state = 5;
                           continue;
                case 0x5c: slash = 1;
                           state = 4;
                           break;
                } break;
        case 3:
        case 4: switch (c) {  // backslash sequence
                case 0x22: buf -= (1 + slash) >> 1;
                           if (slash & 1) {
                               state -= 2;
                               break;
                           } // fallthrough
                default  : cmd -= 1 + (c >= 0x10000);
                           state -= 2;
                           continue;
                case 0x5c: slash++;
                } break;
        case 5: switch (c) {  // quoted token exit
                default  : cmd -= 1 + (c >= 0x10000);
                           state = 1;
                           continue;
                case 0x22: state = 1;
                } break;
        case 6: switch (c) {  // begin argv[0]
                case 0x09:
                case 0x20: *buf++ = 0;
                           state = 0;
                           continue;
                case 0x22: state = 8;
                           continue;
                default  : state = 7;
                } break;
        case 7: switch (c) {  // unquoted argv[0]
                case 0x09:
                case 0x20: *buf++ = 0;
                           state = 0;
                           continue;
                } break;
        case 8: switch (c) {  // quoted argv[0]
                case 0x22: *buf++ = 0;
                           state = 0;
                           continue;
                } break;
        }

        // WTF-8/UTF-8 encoding
        switch ((c >= 0x80) + (c >= 0x800) + (c >= 0x10000)) {
        case 0: *buf++ = (u8)(0x00 | ((c >>  0)     )); break;
        case 1: *buf++ = (u8)(0xc0 | ((c >>  6)     ));
                *buf++ = (u8)(0x80 | ((c >>  0) & 63)); break;
        case 2: *buf++ = (u8)(0xe0 | ((c >> 12)     ));
                *buf++ = (u8)(0x80 | ((c >>  6) & 63));
                *buf++ = (u8)(0x80 | ((c >>  0) & 63)); break;
        case 3: *buf++ = (u8)(0xf0 | ((c >> 18)     ));
                *buf++ = (u8)(0x80 | ((c >> 12) & 63));
                *buf++ = (u8)(0x80 | ((c >>  6) & 63));
                *buf++ = (u8)(0x80 | ((c >>  0) & 63)); break;
        }
    }

    *buf = 0;
    argv[argc] = 0;
    return argc;
}
