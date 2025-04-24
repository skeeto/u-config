#include "src/u-config.c"

typedef long long   i64;

enum {
    WASI_FD_READ        = 1 << 1,
    WASI_FD_READDIR     = 1 << 14,
    WASI_O_DIRECTORY    = 1 << 1,
};

#define WASI(s) \
  __attribute((import_module("wasi_snapshot_preview1"), import_name(s)))
WASI("args_get")            i32  args_get(u8 **, u8 *);
WASI("args_sizes_get")      i32  args_sizes_get(i32 *, iz *);
WASI("environ_get")         i32  environ_get(u8 **, u8 *);
WASI("environ_sizes_get")   i32  environ_sizes_get(i32 *, iz *);
WASI("fd_close")            i32  fd_close(i32);
WASI("fd_prestat_dir_name") i32  fd_prestat_dir_name(i32, u8 *, iz);
WASI("fd_prestat_get")      i32  fd_prestat_get(i32, iz *);
WASI("fd_read")             i32  fd_read(i32, s8 *, iz, iz *);
WASI("fd_readdir")          i32  fd_readdir(i32, u8 *, iz, i64, iz *);
WASI("fd_write")            i32  fd_write(i32, s8 *, iz, iz *);
WASI("path_open")           i32  path_open(i32,i32,u8*,iz,i32,i64,i64,i32,i32*);
WASI("proc_exit")           void proc_exit(i32);

typedef struct dirfd dirfd;
struct dirfd {
    dirfd *next;
    s8     path;
    i32    fd;
};

struct os {
    dirfd *dirs;
};

static b32 endswith_(s8 s, s8 suffix)
{
    return s.len>=suffix.len && s8equals(taketail(s, suffix.len), suffix);
}

static dirfd *find_preopens(arena *a)
{
    dirfd  *head = 0;
    dirfd **tail = &head;
    for (i32 fd = 3;; fd++) {
        iz stat[2];
        if (fd_prestat_get(fd, stat)) {
            return head;
        }

        s8 path = {0};
        path.len = stat[1]+1;
        path.s   = new(a, u8, path.len);
        if (fd_prestat_dir_name(fd, path.s, stat[1])) {
            return head;
        }
        path.s[stat[1]] = '/';

        // Force exactly one trailing slash
        while (endswith_(path, S("//"))) {
            path = cuttail(path, 1);
        }

        *tail = new(a, dirfd, 1);
        (*tail)->path = path;
        (*tail)->fd   = fd;
        tail = &(*tail)->next;
    }
}

typedef struct {
    s8  relpath;
    i32 fd;
    b32 ok;
} relpath;

static relpath find_dirfd(os *ctx, s8 path)
{
    relpath r = {0};
    for (dirfd *d = ctx->dirs; d; d = d->next) {
        // Match without trailing slash
        s8 dir = d->path;
        while (endswith_(dir, S("/"))) {
            dir = cuttail(dir, 1);
        }

        // TODO: parse path and match components
        if (startswith(path, dir)) {
            r.relpath = cuthead(path, dir.len);

            // Remove leading slashes
            while (startswith(r.relpath, S("/"))) {
                r.relpath = cuthead(r.relpath, 1);
            }

            // Empty path really means current directory
            if (!r.relpath.len) {
                r.relpath = S(".");
            }

            r.fd = d->fd;
            r.ok = 1;
            return r;
        }
    }
    return r;
}

static filemap os_mapfile(os *ctx, arena *perm, s8 path)
{
    filemap r = {0};

    path = cuttail(path, 1);  // remove null terminator
    relpath rel = find_dirfd(ctx, path);
    if (!rel.ok) {
        r.status = filemap_NOTFOUND;
        return r;
    }
    path = rel.relpath;

    i32 fd  = -1;
    i32 err = path_open(
        rel.fd, 0, path.s, path.len, 0, WASI_FD_READ, 0, 0, &fd
    );
    if (err) {
        r.status = filemap_NOTFOUND;
        return r;
    }

    iz cap = perm->end - perm->beg;
    r.data.s   = (u8 *)perm->beg;
    r.data.len = cap;
    err = fd_read(fd, &r.data, 1, &r.data.len);
    fd_close(fd);
    if (err || r.data.len==cap) {
        r.status = filemap_READERR;
        return r;
    }

    perm->beg += r.data.len;
    r.status = filemap_OK;
    return r;
}

static s8node *os_listing(os *ctx, arena *a, s8 path)
{
    s8list r = {0};

    path = cuttail(path, 1);  // remove null terminator
    relpath rel = find_dirfd(ctx, path);
    if (!rel.ok) {
        return 0;
    }
    path = rel.relpath;

    i32 fd  = -1;
    i32 err = path_open(
        rel.fd, 0,
        path.s, path.len,
        WASI_O_DIRECTORY, WASI_FD_READDIR, 0, 0, &fd
    );
    if (err) {
        return 0;
    }

    i64  cookie = 0;
    i32  cap    = 1<<14;
    u8  *buf    = new(a, u8, cap);
    for (;;) {
        iz len = 0;
        err = fd_readdir(fd, buf, cap, cookie, &len);
        if (err || !len) {
            break;
        }

        for (iz off = 0; off < len;) {
            struct {
                i64 cookie;
                i64 inode;
                i32 len;
                u8  type;
            } entry;

            // NOTE: fd_readdir() returns unaligned data (!!!)
            __builtin_memcpy(&entry, buf+off, sizeof(entry));
            off += sizeof(entry);

            s8 name = {buf+off, entry.len};
            off += entry.len;

            if (off > len) {
                break;  // truncated
            }
            cookie = entry.cookie;

            if (endswith_(name, S(".pc"))) {
                s8 copy = news8(a, name.len);
                s8copy(copy, name);
                append(&r, copy, a);
            }
        }
    }

    fd_close(fd);
    return r.head;
}

static void os_write(os *ctx, i32 fd, s8 data)
{
    if (fd_write(fd, &data, 1, &data.len)) {
        os_fail(ctx);
    }
}

static void os_fail(os *ctx)
{
    (void)ctx;
    proc_exit(1);
    __builtin_unreachable();
}

void _start(void)
{
    os ctx = {0};

    static byte heap[1<<22];
    arena perm = {0};
    perm.beg = heap;
    perm.end = heap + sizeof(heap);
    perm.ctx = &ctx;

    ctx.dirs = find_preopens(&perm);

    i32 argc   = 0;
    iz  buflen = 0;
    args_sizes_get(&argc, &buflen);

    u8 **argv = new(&perm, u8 *, argc);
    u8  *buf  = new(&perm, u8, buflen);
    args_get(argv, buf);

    i32 envc   = 0;
    iz  envlen = 0;
    environ_sizes_get(&envc, &envlen);

    u8 **envp = new(&perm, u8 *, envc);
    u8  *env  = new(&perm, u8, envlen);
    environ_get(envp, env);

    config conf = {0};
    conf.perm          = perm;
    conf.args          = argv + !!argc;
    conf.nargs         = argc - !!argc;
    conf.pc_path       = S("/usr/lib/pkgconfig:/usr/share/pkgconfig");
    conf.fixedpath     = conf.pc_path;
    conf.pc_sysincpath = conf.sys_incpath = S("/usr/include");
    conf.pc_syslibpath = conf.sys_libpath = S("/usr/lib");
    conf.delim         = ':';
    conf.haslisting    = 1;

    for (i32 i = 0; i < envc; i++) {
        cut c = s8cut(s8fromcstr(envp[i]), '=');
        s8 name = c.head;
        s8 value = c.tail;

        if (s8equals(name, S("PKG_CONFIG_PATH"))) {
            conf.envpath = value;
        } else if (s8equals(name, S("PKG_CONFIG_LIBDIR"))) {
            conf.fixedpath = value;
        } else if (s8equals(name, S("PKG_CONFIG_TOP_BUILD_DIR"))) {
            conf.top_builddir = value;
        } else if (s8equals(name, S("PKG_CONFIG_SYSTEM_INCLUDE_PATH"))) {
            conf.sys_incpath = value;
        } else if (s8equals(name, S("PKG_CONFIG_SYSTEM_LIBRARY_PATH"))) {
            conf.sys_libpath = value;
        } else if (s8equals(name, S("PKG_CONFIG_ALLOW_SYSTEM_CFLAGS"))) {
            conf.print_sysinc = value;
        } else if (s8equals(name, S("PKG_CONFIG_ALLOW_SYSTEM_LIBS"))) {
            conf.print_syslib = value;
        }
    }

    uconfig(&conf);
}
