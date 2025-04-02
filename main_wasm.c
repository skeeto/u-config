#include "src/u-config.c"

__attribute((import_name("write"))) void wasm_write(i32, u8 *, iz);

// https://github.com/WebAssembly/tool-conventions/blob/main/SetjmpLongjmp.md
i32  setjmp(void *);        // becomes __wasm_setjmp
void longjmp(void *, i32);  // becomes __wasm_longjmp + __wasm_setjmp_test

struct os {
    arena perm;
    env  *filesystem;
    u8  **args;
    i32   nargs;
    void *jmpbuf[4];
};

static filemap os_mapfile(os *ctx, arena *a, s8 path)
{
    (void)a;

    filemap r = {0};
    s8 *data = insert(&ctx->filesystem, cuttail(path, 1), 0);
    if (!data) {
        r.status = filemap_NOTFOUND;
        return r;
    }
    r.data = *data;
    return r;
}

static s8list os_listing_(s8list r, env *fs, arena *a, s8 path)
{
    if (fs) {
        if (startswith(fs->name, path)) {
            append(&r, cuthead(fs->name, path.len+1), a);
        }
        for (i32 i = 0; i < 4; i++) {
            r = os_listing_(r, fs->child[i], a, path);
        }
    }
    return r;
}

static s8node *os_listing(os *ctx, arena *a, s8 path)
{
    assert(path.s);
    assert(path.len);
    assert(!path.s[path.len-1]);
    s8list r = {0};
    r = os_listing_(r, ctx->filesystem, a, cuttail(path, 1));
    return r.head;
}

static void os_write(os *_, i32 fd, s8 data)
{
    wasm_write(fd, data.s, data.len);
}

static void os_fail(os *ctx)
{
    // It's a hacky mess, but it works.
    longjmp(ctx->jmpbuf, 1);
    __builtin_wasm_throw(1, ctx->jmpbuf+2);
}

static os ctx;

// Free all prior allocations and reset this WASM instance to a clean
// state. If uconfig exited via abort, this won't be enough.
__attribute((export_name("initialize")))
void wasm_initialize(void)
{
    static byte heap[1<<21];
    ctx.perm.beg = heap;
    ctx.perm.end = heap + sizeof(heap);
    ctx.perm.ctx = &ctx;
    ctx.filesystem = 0;
    ctx.args = 0;
    ctx.nargs = 0;
}

// Allocate space for a string, including s8 header. Allocates an extra
// null byte just beyond the end so that the string is null-terminated.
__attribute((export_name("pushstring")))
s8 *wasm_pushstring(iz len)
{
    if (len >= 0x7fffffff) oom(0);
    s8* r  = new(&ctx.perm, s8, 1);
    r->s   = new(&ctx.perm, u8, len+1);
    r->len = len;
    return r;
}

__attribute((export_name("pushfile")))
void wasm_pushfile(s8 *name, s8 *contents)
{
    s8 prefix = S("/usr/lib/pkgconfig/");
    s8 path   = news8(&ctx.perm, prefix.len+name->len);  // cannot overflow
    s8copy(s8copy(path, prefix), *name);
    *insert(&ctx.filesystem, path, &ctx.perm) = *contents;
}

__attribute((export_name("pushargs")))
void wasm_pushargs(i32 len)
{
    ctx.args  = new(&ctx.perm, u8 *, len);
    ctx.nargs = 0;
}

__attribute((export_name("pusharg")))
void wasm_pusharg(s8 *arg)
{
    ctx.args[ctx.nargs++] = arg->s;
}

__attribute((export_name("uconfig")))
i32 wasm_uconfig(void)
{
    config conf = {0};
    conf.perm = ctx.perm;
    conf.args = ctx.args;
    conf.nargs = ctx.nargs;
    conf.pc_path = conf.fixedpath = S("/usr/lib/pkgconfig");
    conf.pc_sysincpath = conf.sys_incpath = S("/usr/include");
    conf.pc_syslibpath = conf.sys_libpath = S("/usr/lib");
    conf.delim = ':';
    conf.haslisting = 1;

    if (setjmp(ctx.jmpbuf)) {
        return 2;  // use standard error
    }
    uconfig(&conf);
    return 1;  // use standard output
}
