// ls.c
// Full-featured POSIX-like ls for Windows

#define _CRT_SECURE_NO_WARNINGS
#define _UNICODE

#include <windows.h>
#include <shlwapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <tchar.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#include <accctrl.h>
#include <aclapi.h>
#include <stdbool.h>
#include <locale.h>
#include <sddl.h>
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif

#define VERSION L"Windows ls port by penguincre8te (1.0)"
// ----------------------------- Utilities -----------------------------------

static void enable_virtual_terminal() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD mode = 0;
    if (!GetConsoleMode(hOut, &mode)) return;
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
    SetConsoleMode(hOut, mode);
}

static void print_help_and_exit(const wchar_t *prog) {
    // Print a condensed but complete --help text (mirrors GNU ls description)
    wprintf(L"Usage: %s [OPTION]... [FILE]...\n", prog);
    wprintf(
        L"List information about the FILEs (the current directory by default).\n\n"
        L"Mandatory arguments to long options are mandatory for short options too.\n\n"
        L"  -a, --all                 do not ignore entries starting with .\n"
        L"  -A, --almost-all          do not list implied . and ..\n"
        L"      --author              with -l, print the author of each file\n"
        L"  -b, --escape              print C-style escapes for nongraphic characters\n"
        L"      --block-size=SIZE     with -l, scale sizes by SIZE when printing them\n"
        L"  -B, --ignore-backups      do not list implied entries ending with ~\n"
        L"  -c                        with -lt: sort by, and show, ctime; with -l: show ctime and sort by name; otherwise: sort by ctime\n"
        L"  -C                        list entries by columns\n"
        L"      --color[=WHEN]        color the output WHEN: never, auto, always\n"
        L"  -d, --directory           list directories themselves, not their contents\n"
        L"  -D, --dired               generate output designed for Emacs' dired mode\n"
        L"  -f                        same as -a -U\n"
        L"  -F, --classify[=WHEN]     append indicator (one of */=>@|) to entries\n"
        L"      --file-type           likewise, except do not append '*'\n"
        L"      --format=WORD         across,horizontal (-x), commas (-m), long (-l), single-column (-1), verbose (-l), vertical (-C)\n"
        L"      --full-time           like -l --time-style=full-iso\n"
        L"  -g                        like -l, but do not list owner\n"
        L"      --group-directories-first  group directories before files\n"
        L"  -G, --no-group            in a long listing, don't print group names\n"
        L"  -h, --human-readable      with -l and -s, print sizes like 1K 234M 2G etc.\n"
        L"      --si                  likewise, but use powers of 1000 not 1024\n"
        L"  -H, --dereference-command-line  follow symbolic links listed on the command line\n"
        L"      --dereference-command-line-symlink-to-dir  follow each command line symbolic link that points to a directory\n"
        L"      --hide=PATTERN        do not list implied entries matching shell PATTERN (overridden by -a or -A)\n"
        L"      --hyperlink[=WHEN]    hyperlink file names WHEN\n"
        L"      --indicator-style=WORD  append indicator with style WORD to entry names: none, slash (-p), file-type (--file-type), classify (-F)\n"
        L"  -i, --inode               print the index number of each file\n"
        L"  -I, --ignore=PATTERN      do not list implied entries matching shell PATTERN\n"
        L"  -k, --kibibytes           default to 1024-byte blocks for file system usage; used only with -s and per directory totals\n"
        L"  -l                        use a long listing format\n"
        L"  -L, --dereference         when showing file information for a symbolic link, show information for the file the link references\n"
        L"  -m                        fill width with a comma separated list of entries\n"
        L"  -n, --numeric-uid-gid     like -l, but list numeric user and group IDs\n"
        L"  -N, --literal             print entry names without quoting\n"
        L"  -o                        like -l, but do not list group information\n"
        L"  -p, --indicator-style=slash  append / indicator to directories\n"
        L"  -q, --hide-control-chars  print ? instead of nongraphic characters\n"
        L"      --show-control-chars  show nongraphic characters as-is (the default)\n"
        L"  -Q, --quote-name          enclose entry names in double quotes\n"
        L"      --quoting-style=WORD  use quoting style WORD for entry names: literal, locale, shell, shell-always, shell-escape, shell-escape-always, c, escape\n"
        L"  -r, --reverse             reverse order while sorting\n"
        L"  -R, --recursive           list subdirectories recursively\n"
        L"  -s, --size                print the allocated size of each file, in blocks\n"
        L"  -S                        sort by file size, largest first\n"
        L"      --sort=WORD           change default 'name' sort to WORD: none (-U), size (-S), time (-t), version (-v), extension (-X), name, width\n"
        L"      --time=WORD           select which timestamp used to display or sort: atime, ctime, mtime, birth\n"
        L"      --time-style=STYLE    time/date format with -l: full-iso, long-iso, iso, locale, +FORMAT\n"
        L"  -t                        sort by time, newest first\n"
        L"  -T, --tabsize=COLS        assume tab stops at each COLS instead of 8\n"
        L"  -u                        with -lt: sort by, and show, access time; with -l: show access time and sort by name; otherwise: sort by access time\n"
        L"  -U                        do not sort directory entries\n"
        L"  -v                        natural sort of (version) numbers within text\n"
        L"  -w, --width=COLS          set output width to COLS.  0 means no limit\n"
        L"  -x                        list entries by lines instead of by columns\n"
        L"  -X                        sort alphabetically by entry extension\n"
        L"  -Z, --context             print any security context of each file\n"
        L"      --zero                end each output line with NUL, not newline\n"
        L"  -1                        list one file per line\n"
        L"      --help                display this help and exit\n"
        L"      --version             output version information and exit\n\n"
        L"SIZE argument: integer and optional unit (K,M,G,...). TIME_STYLE: full-iso, long-iso, iso, locale, +FORMAT.\n"
    );
    exit(0);
}

static void print_version_and_exit() {
    wprintf(VERSION);
    exit(0);
}

// ----------------------------- Option parsing --------------------------------

typedef enum { SORT_NAME, SORT_NONE, SORT_SIZE, SORT_TIME, SORT_VERSION, SORT_EXTENSION } SortMode;
typedef enum { TIME_MTIME, TIME_ATIME, TIME_CTIME, TIME_BIRTH } TimeKind;
typedef enum { QUOTE_LITERAL, QUOTE_LOCALE, QUOTE_SHELL, QUOTE_SHELL_ALWAYS, QUOTE_SHELL_ESCAPE, QUOTE_SHELL_ESCAPE_ALWAYS, QUOTE_C, QUOTE_ESCAPE } QuotingStyle;

typedef struct {
    bool opt_all;
    bool opt_almost_all;
    bool opt_long;
    bool opt_recursive;
    bool opt_human;
    bool opt_si;
    bool opt_one;
    bool opt_comma;
    bool opt_inode;
    bool opt_numeric_ids;
    bool opt_literal;
    bool opt_quote;
    bool opt_hide_control;
    bool opt_dereference;
    bool opt_classify;
    bool opt_file_type;
    bool opt_indicator_slash;
    bool opt_ignore_backups;
    bool opt_author;
    bool opt_no_group;
    bool opt_group_dirs_first;
    bool opt_dired;
    bool opt_full_time;
    bool opt_kibibytes;
    bool opt_deref_cmdline;
    bool opt_deref_cmdline_symlink_to_dir;
    bool opt_zero;
    bool opt_reverse;
    bool opt_unsorted;
    bool opt_size_alloc;
    bool opt_tabsize_set;
    int tabsize;
    SortMode sort;
    TimeKind time_kind;
    QuotingStyle quoting;
    bool opt_hyperlink;
    bool opt_hyperlink_auto;
    bool opt_color_always;
    bool opt_color_auto;
    bool opt_color_never;
    bool opt_show_context;
    bool opt_dont_sort;
    bool opt_version_sort;
    bool opt_width_set;
    int width;
    wchar_t block_size_str[64];
    unsigned long long block_size; // parsed
    wchar_t time_style[256];
    wchar_t hide_pattern[256];
    wchar_t ignore_pattern[256];
    wchar_t indicator_style[64];
    bool opt_format_set;
    wchar_t format_word[64];
    bool opt_file_type_only;
    QuotingStyle quoting_style;
} Options;

static Options opts;

static void init_options() {
    memset(&opts, 0, sizeof(opts));
    opts.sort = SORT_NAME;
    opts.time_kind = TIME_MTIME;
    opts.tabsize = 8;
    opts.block_size = 0;
    opts.quoting = QUOTE_LOCALE;
    opts.quoting_style = QUOTE_LOCALE;
    opts.width = 80;
}

static bool starts_with(const wchar_t *s, const wchar_t *p) {
    return wcsncmp(s, p, wcslen(p)) == 0;
}

static unsigned long long parse_size_suffix(const wchar_t *s, bool si) {
    // parse integer optionally followed by K M G T P E Z Y (powers)
    wchar_t *end = NULL;
    unsigned long long val = wcstoull(s, &end, 10);
    if (!end || *end == L'\0') return val;
    wchar_t unit = towupper(*end);
    unsigned long long base = si ? 1000ULL : 1024ULL;
    unsigned long long mul = 1;
    switch (unit) {
        case L'K': mul = base; break;
        case L'M': mul = base * base; break;
        case L'G': mul = base * base * base; break;
        case L'T': mul = base * base * base * base; break;
        case L'P': mul = base * base * base * base * base; break;
        default: mul = 1; break;
    }
    return val * mul;
}

// ----------------------------- Quoting / printing helpers --------------------

static void print_escaped_wchar(wchar_t wc) {
    // C-style escapes for non-graphic
    if (wc == L'\n') { fputwc(L'\\', stdout); fputwc(L'n', stdout); return; }
    if (wc == L'\t') { fputwc(L'\\', stdout); fputwc(L't', stdout); return; }
    if (wc < 32 || wc == 127) {
        // \xHH
        fwprintf(stdout, L"\\x%02x", (unsigned int)wc);
        return;
    }
    fputwc(wc, stdout);
}

static void print_name_quoting(const wchar_t *name) {
    if (opts.opt_literal) {
        fputws(name, stdout);
        return;
    }
    if (opts.quoting_style == QUOTE_C) {
        // C-style: double quotes, escape backslash and double quote and non-graphic
        fputwc(L'"', stdout);
        for (const wchar_t *p = name; *p; ++p) {
            if (*p == L'\\' || *p == L'"') { fputwc(L'\\', stdout); fputwc(*p, stdout); }
            else if (opts.opt_hide_control && *p < 32) { fputwc(L'?', stdout); }
            else fputwc(*p, stdout);
        }
        fputwc(L'"', stdout);
        return;
    }
    if (opts.opt_quote) {
        fputwc(L'"', stdout);
        for (const wchar_t *p = name; *p; ++p) {
            if (*p == L'"' || *p == L'\\') { fputwc(L'\\', stdout); fputwc(*p, stdout); }
            else if (opts.opt_hide_control && *p < 32) fputwc(L'?', stdout);
            else fputwc(*p, stdout);
        }
        fputwc(L'"', stdout);
        return;
    }
    // default: print with control char handling
    for (const wchar_t *p = name; *p; ++p) {
        if (opts.opt_hide_control && *p < 32) fputwc(L'?', stdout);
        else fputwc(*p, stdout);
    }
}

// ----------------------------- Color handling --------------------------------

static bool is_output_tty() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return false;
    DWORD mode;
    return GetConsoleMode(hOut, &mode) != 0;
}

static void set_color_by_type(const WIN32_FIND_DATAW *fd) {
    // Basic mapping: directories blue, symlink cyan, executable green, socket/pipe yellow, regular default
    if (fd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        wprintf(L"\x1b[34m"); // blue
    } else if (fd->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
        wprintf(L"\x1b[36m"); // cyan
    } else {
        // check extension
        const wchar_t *name = fd->cFileName;
        const wchar_t *ext = wcsrchr(name, L'.');
        if (ext && (_wcsicmp(ext, L".exe") == 0 || _wcsicmp(ext, L".com") == 0 || _wcsicmp(ext, L".bat") == 0 || _wcsicmp(ext, L".cmd") == 0)) {
            wprintf(L"\x1b[32m"); // green
        } else {
            // default
        }
    }
}

static void reset_color() {
    wprintf(L"\x1b[0m");
}

// ----------------------------- File info helpers -----------------------------

static unsigned long long combine_file_index(const BY_HANDLE_FILE_INFORMATION *info) {
    return ((unsigned long long)info->nFileIndexHigh << 32) | info->nFileIndexLow;
}

static void filetime_to_localstr(const FILETIME *ft, wchar_t *buf, size_t bufsz, const wchar_t *style) {
    // style: "full-iso", "long-iso", "iso", "locale", or +FORMAT (strftime-like)
    SYSTEMTIME stUTC, stLocal;
    FileTimeToSystemTime(ft, &stUTC);
    SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
    if (style && wcslen(style) > 0) {
        if (wcscmp(style, L"full-iso") == 0) {
            swprintf(buf, bufsz, L"%04d-%02d-%02d %02d:%02d:%02d.%03d",
                     stLocal.wYear, stLocal.wMonth, stLocal.wDay,
                     stLocal.wHour, stLocal.wMinute, stLocal.wSecond, stLocal.wMilliseconds);
            return;
        } else if (wcscmp(style, L"long-iso") == 0) {
            swprintf(buf, bufsz, L"%04d-%02d-%02d %02d:%02d",
                     stLocal.wYear, stLocal.wMonth, stLocal.wDay,
                     stLocal.wHour, stLocal.wMinute);
            return;
        } else if (wcscmp(style, L"iso") == 0) {
            swprintf(buf, bufsz, L"%04d-%02d-%02d %02d:%02d",
                     stLocal.wYear, stLocal.wMonth, stLocal.wDay,
                     stLocal.wHour, stLocal.wMinute);
            return;
        } else if (style[0] == L'+') {
            // use wcsftime-like formatting by converting to time_t
            SYSTEMTIME st = stLocal;
            struct tm tm;
            tm.tm_year = st.wYear - 1900;
            tm.tm_mon = st.wMonth - 1;
            tm.tm_mday = st.wDay;
            tm.tm_hour = st.wHour;
            tm.tm_min = st.wMinute;
            tm.tm_sec = st.wSecond;
            tm.tm_isdst = -1;
            time_t t = _mkgmtime(&tm); // approximate
            // convert wide format to narrow for strftime
            char fmt[256]; wcstombs(fmt, style+1, sizeof(fmt)-1);
            char out[256]; strftime(out, sizeof(out), fmt, gmtime(&t));
            wchar_t wout[256]; mbstowcs(wout, out, sizeof(wout)/sizeof(wchar_t)-1);
            wcsncpy(buf, wout, bufsz-1); buf[bufsz-1] = L'\0';
            return;
        }
    }
    // default
    swprintf(buf, bufsz, L"%04d-%02d-%02d %02d:%02d",
             stLocal.wYear, stLocal.wMonth, stLocal.wDay,
             stLocal.wHour, stLocal.wMinute);
}

// ----------------------------- Pattern matching ------------------------------

static bool match_pattern(const wchar_t *pattern, const wchar_t *name) {
    // Use PathMatchSpecW from shlwapi
    return PathMatchSpecW(name, pattern) == TRUE;
}

// ----------------------------- Sorting helpers -------------------------------

static int natural_compare(const wchar_t *a, const wchar_t *b) {
    // simple natural comparator: compare numeric runs numerically
    const wchar_t *pa = a, *pb = b;
    while (*pa && *pb) {
        if (iswdigit(*pa) && iswdigit(*pb)) {
            unsigned long long na = 0, nb = 0;
            while (iswdigit(*pa)) { na = na*10 + (*pa - L'0'); ++pa; }
            while (iswdigit(*pb)) { nb = nb*10 + (*pb - L'0'); ++pb; }
            if (na < nb) return -1;
            if (na > nb) return 1;
        } else {
            wchar_t ca = towlower(*pa), cb = towlower(*pb);
            if (ca < cb) return -1;
            if (ca > cb) return 1;
            ++pa; ++pb;
        }
    }
    if (*pa) return 1;
    if (*pb) return -1;
    return 0;
}

// ----------------------------- Main listing logic ---------------------------

typedef struct {
    wchar_t name[PATH_MAX];
    wchar_t full[PATH_MAX];
    WIN32_FIND_DATAW fd;
    unsigned long long inode;
    unsigned long long size;
    FILETIME times[3]; // mtime, atime, ctime(creation)
} Entry;

static int cmp_name(const void *pa, const void *pb) {
    const Entry *a = pa, *b = pb;
    return _wcsicmp(a->name, b->name);
}
static int cmp_name_rev(const void *pa, const void *pb) {
    return -cmp_name(pa,pb);
}
static int cmp_size(const void *pa, const void *pb) {
    const Entry *a = pa, *b = pb;
    if (a->size < b->size) return 1;
    if (a->size > b->size) return -1;
    return _wcsicmp(a->name, b->name);
}
static int cmp_time(const void *pa, const void *pb) {
    const Entry *a = pa, *b = pb;
    unsigned long long ta = (((unsigned long long)a->times[0].dwHighDateTime) << 32) | a->times[0].dwLowDateTime;
    unsigned long long tb = (((unsigned long long)b->times[0].dwHighDateTime) << 32) | b->times[0].dwLowDateTime;
    if (ta < tb) return 1;
    if (ta > tb) return -1;
    return _wcsicmp(a->name, b->name);
}
static int cmp_version(const void *pa, const void *pb) {
    const Entry *a = pa, *b = pb;
    return natural_compare(a->name, b->name);
}
static int cmp_extension(const void *pa, const void *pb) {
    const Entry *a = pa, *b = pb;
    const wchar_t *ea = wcsrchr(a->name, L'.');
    const wchar_t *eb = wcsrchr(b->name, L'.');
    if (!ea && !eb) return _wcsicmp(a->name, b->name);
    if (!ea) return -1;
    if (!eb) return 1;
    int r = _wcsicmp(ea, eb);
    if (r == 0) return _wcsicmp(a->name, b->name);
    return r;
}

static int gather_entries(const wchar_t *dir, Entry **out, size_t *outn) {
    wchar_t search[PATH_MAX];
    if (wcslen(dir) == 0) wcscpy(search, L"*");
    else {
        swprintf(search, PATH_MAX, L"%s\\*", dir);
    }
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(search, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return -1;
    }
    Entry *arr = NULL; size_t cap = 0, n = 0;
    do {
        const wchar_t *name = fd.cFileName;
        if (!opts.opt_all && !opts.opt_almost_all) {
            if (name[0] == L'.') continue;
        }
        if (opts.opt_almost_all) {
            if (wcscmp(name, L".") == 0 || wcscmp(name, L"..") == 0) continue;
        }
        if (opts.opt_ignore_backups) {
            size_t L = wcslen(name);
            if (L > 0 && name[L-1] == L'~') continue;
        }
        if (opts.hide_pattern[0]) {
            if (match_pattern(opts.hide_pattern, name)) continue;
        }
        if (opts.ignore_pattern[0]) {
            if (match_pattern(opts.ignore_pattern, name)) continue;
        }
        if (n + 1 > cap) {
            cap = cap ? cap * 2 : 256;
            arr = realloc(arr, cap * sizeof(Entry));
        }
        Entry *e = &arr[n++];
        wcscpy(e->name, name);
        if (wcslen(dir) == 0) swprintf(e->full, PATH_MAX, L"%s", name);
        else swprintf(e->full, PATH_MAX, L"%s\\%s", dir, name);
        e->fd = fd;
        e->size = ((unsigned long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
        e->times[0] = fd.ftLastWriteTime;
        e->times[1] = fd.ftLastAccessTime;
        e->times[2] = fd.ftCreationTime;
        // get inode
        HANDLE fh = CreateFileW(e->full, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
        e->inode = 0;
        if (fh != INVALID_HANDLE_VALUE) {
            BY_HANDLE_FILE_INFORMATION info;
            if (GetFileInformationByHandle(fh, &info)) {
                e->inode = combine_file_index(&info);
            }
            CloseHandle(fh);
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    *out = arr; *outn = n;
    return 0;
}

static void print_long_entry(const Entry *e) {
    // mode/type
    DWORD attrs = e->fd.dwFileAttributes;
    wchar_t mode[16];
    wchar_t t = L'-';
    if (attrs & FILE_ATTRIBUTE_DIRECTORY) t = L'd';
    else if (attrs & FILE_ATTRIBUTE_REPARSE_POINT) t = L'l';
    else t = L'-';
    wchar_t perm[10] = L"rwxrwxrwx";
    if (attrs & FILE_ATTRIBUTE_READONLY) {
        perm[2] = perm[5] = perm[8] = L'-';
    }
    // exec bit heuristic
    const wchar_t *name = e->name;
    const wchar_t *ext = wcsrchr(name, L'.');
    if (!(attrs & FILE_ATTRIBUTE_DIRECTORY) && !(ext && (_wcsicmp(ext, L".exe")==0 || _wcsicmp(ext, L".com")==0 || _wcsicmp(ext, L".bat")==0 || _wcsicmp(ext, L".cmd")==0))) {
        perm[2] = perm[5] = perm[8] = L'-';
    }
    swprintf(mode, 16, L"%c%c%c%c%c%c%c%c%c%c", t, perm[0],perm[1],perm[2],perm[3],perm[4],perm[5],perm[6],perm[7],perm[8]);

    // link count and owner
    unsigned long nlinks = 1;
    wchar_t owner[256] = L"UNKNOWN";
    HANDLE fh = CreateFileW(e->full, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (fh != INVALID_HANDLE_VALUE) {
        BY_HANDLE_FILE_INFORMATION info;
        if (GetFileInformationByHandle(fh, &info)) {
            nlinks = info.nNumberOfLinks;
            // owner
            PSECURITY_DESCRIPTOR sd = NULL;
            PSID ownerSid = NULL;
            if (GetSecurityInfo(fh, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION, &ownerSid, NULL, NULL, NULL, &sd) == ERROR_SUCCESS) {
                // try to lookup name
                wchar_t namebuf[256]; wchar_t dom[256]; DWORD nc = 256, nd = 256;
                SID_NAME_USE use;
                if (LookupAccountSidW(NULL, ownerSid, namebuf, &nc, dom, &nd, &use)) {
                    swprintf(owner, 256, L"%s\\%s", dom, namebuf);
                } else {
                    LPWSTR sidstr = NULL;
                    if (ConvertSidToStringSidW(ownerSid, &sidstr)) {
                        wcsncpy(owner, sidstr, 255);
                        LocalFree(sidstr);
                    }
                }
                if (sd) LocalFree(sd);
            }
        }
        CloseHandle(fh);
    }

    // size
    wchar_t sizestr[64];
    if (opts.opt_human) {
        double s = (double)e->size;
        const wchar_t *units[] = {L"B", L"K", L"M", L"G", L"T", L"P"};
        int i = 0; double base = opts.opt_si ? 1000.0 : 1024.0;
        while (s >= base && i < 5) { s /= base; ++i; }
        if (i == 0) swprintf(sizestr, 64, L"%llu%s", e->size, units[i]);
        else swprintf(sizestr, 64, L"%.1f%s", s, units[i]);
    } else {
        swprintf(sizestr, 64, L"%llu", e->size);
    }

    // time
    wchar_t timestr[128];
    FILETIME *ft = (FILETIME*)&e->times[0];
    switch (opts.time_kind) {
        case TIME_ATIME: ft = (FILETIME*)&e->times[1]; break;
        case TIME_BIRTH: ft = (FILETIME*)&e->times[2]; break;
        case TIME_CTIME: ft = (FILETIME*)&e->times[2]; break;
        case TIME_MTIME:
        default: ft = (FILETIME*)&e->times[0]; break;
    }
    filetime_to_localstr(ft, timestr, sizeof(timestr)/sizeof(wchar_t), opts.time_style);

    // print
    wprintf(L"%s %3lu %s %8s %s ", mode, (unsigned long)nlinks, owner, sizestr, timestr);
    // color
    bool colored = false;
    if ((opts.opt_color_always) || (opts.opt_color_auto && is_output_tty())) {
        set_color_by_type(&e->fd);
        colored = true;
    }
    print_name_quoting(e->name);
    if (opts.opt_classify) {
        if (e->fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) wprintf(L"/");
        else if (e->fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) wprintf(L"@");
        else {
            const wchar_t *ext = wcsrchr(e->name, L'.');
            if (ext && (_wcsicmp(ext, L".exe")==0 || _wcsicmp(ext, L".com")==0 || _wcsicmp(ext, L".bat")==0 || _wcsicmp(ext, L".cmd")==0)) wprintf(L"*");
        }
    } else if (opts.opt_indicator_slash) {
        if (e->fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) wprintf(L"/");
    }
    if (opts.opt_inode && e->inode) wprintf(L" [%llu]", e->inode);
    if (colored) reset_color();
    if (opts.opt_zero) fputwc(L'\0', stdout);
    else fputwc(L'\n', stdout);
}

static void print_short_entry(const Entry *e) {
    bool colored = false;
    if ((opts.opt_color_always) || (opts.opt_color_auto && is_output_tty())) {
        set_color_by_type(&e->fd);
        colored = true;
    }
    print_name_quoting(e->name);
    if (opts.opt_classify) {
        if (e->fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) wprintf(L"/");
        else if (e->fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) wprintf(L"@");
        else {
            const wchar_t *ext = wcsrchr(e->name, L'.');
            if (ext && (_wcsicmp(ext, L".exe")==0 || _wcsicmp(ext, L".com")==0 || _wcsicmp(ext, L".bat")==0 || _wcsicmp(ext, L".cmd")==0)) wprintf(L"*");
        }
    } else if (opts.opt_indicator_slash) {
        if (e->fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) wprintf(L"/");
    }
    if (colored) reset_color();
    if (opts.opt_zero) fputwc(L'\0', stdout);
    else fputwc(L'\n', stdout);
}

static int list_dir_recursive(const wchar_t *dir, bool print_header) {
    Entry *entries = NULL; size_t n = 0;
    if (gather_entries(dir, &entries, &n) != 0) {
        fwprintf(stderr, L"ls: cannot open directory '%s'\n", dir);
        return 2;
    }
    // sorting
    if (!opts.opt_unsorted) {
        if (opts.sort == SORT_NAME) qsort(entries, n, sizeof(Entry), cmp_name);
        else if (opts.sort == SORT_SIZE) qsort(entries, n, sizeof(Entry), cmp_size);
        else if (opts.sort == SORT_TIME) qsort(entries, n, sizeof(Entry), cmp_time);
        else if (opts.sort == SORT_VERSION) qsort(entries, n, sizeof(Entry), cmp_version);
        else if (opts.sort == SORT_EXTENSION) qsort(entries, n, sizeof(Entry), cmp_extension);
    }
    if (opts.opt_reverse) {
        // reverse array
        for (size_t i = 0; i < n/2; ++i) {
            Entry tmp = entries[i];
            entries[i] = entries[n-1-i];
            entries[n-1-i] = tmp;
        }
    }
    if (print_header) wprintf(L"%s:\n", dir);
    // print entries
    for (size_t i = 0; i < n; ++i) {
        Entry *e = &entries[i];
        if (opts.opt_long) print_long_entry(e);
        else print_short_entry(e);
    }
    // recursion
    if (opts.opt_recursive) {
        for (size_t i = 0; i < n; ++i) {
            Entry *e = &entries[i];
            if (wcscmp(e->name, L".")==0 || wcscmp(e->name, L"..")==0) continue;
            if (e->fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                wprintf(L"\n%s:\n", e->full);
                int rc = list_dir_recursive(e->full, false);
                if (rc != 0) { free(entries); return rc; }
            }
        }
    }
    free(entries);
    return 0;
}

// ----------------------------- Main ----------------------------------------

int wmain(int argc, wchar_t **argv) {
    setlocale(LC_ALL, "");
    init_options();
    enable_virtual_terminal();

    // parse args (basic long/short parsing)
    int i = 1;
    bool paths_given = false;
    wchar_t **paths = NULL; int paths_count = 0;
    while (i < argc) {
        wchar_t *a = argv[i];
        if (wcscmp(a, L"--") == 0) { ++i; break; }
        if (a[0] != L'-' || wcscmp(a, L"-") == 0) break;
        // long options
        if (a[1] == L'-') {
            if (wcscmp(a, L"--help") == 0) print_help_and_exit(argv[0]);
            if (wcscmp(a, L"--version") == 0) print_version_and_exit();
            if (starts_with(a, L"--time=")) {
                wcscpy(opts.time_style, L"");
                wchar_t *val = a + 7;
                if (wcscmp(val, L"atime")==0) opts.time_kind = TIME_ATIME;
                else if (wcscmp(val, L"ctime")==0) opts.time_kind = TIME_CTIME;
                else if (wcscmp(val, L"mtime")==0) opts.time_kind = TIME_MTIME;
                else if (wcscmp(val, L"birth")==0) opts.time_kind = TIME_BIRTH;
                else { fwprintf(stderr, L"ls: invalid --time value '%s'\n", val); return 2; }
                ++i; continue;
            }
            if (starts_with(a, L"--time-style=")) {
                wcscpy(opts.time_style, a + wcslen(L"--time-style="));
                ++i; continue;
            }
            if (starts_with(a, L"--block-size=")) {
                wcscpy(opts.block_size_str, a + wcslen(L"--block-size="));
                opts.block_size = parse_size_suffix(opts.block_size_str, opts.opt_si);
                ++i; continue;
            }
            if (starts_with(a, L"--hide=")) {
                wcscpy(opts.hide_pattern, a + wcslen(L"--hide="));
                ++i; continue;
            }
            if (starts_with(a, L"--ignore=")) {
                wcscpy(opts.ignore_pattern, a + wcslen(L"--ignore="));
                ++i; continue;
            }
            if (starts_with(a, L"--indicator-style=")) {
                wcscpy(opts.indicator_style, a + wcslen(L"--indicator-style="));
                if (wcscmp(opts.indicator_style, L"slash")==0) opts.opt_indicator_slash = true;
                else if (wcscmp(opts.indicator_style, L"file-type")==0) opts.opt_file_type = true;
                else if (wcscmp(opts.indicator_style, L"classify")==0) opts.opt_classify = true;
                ++i; continue;
            }
            if (starts_with(a, L"--format=")) {
                wcscpy(opts.format_word, a + wcslen(L"--format="));
                opts.opt_format_set = true;
                ++i; continue;
            }
            if (starts_with(a, L"--color")) {
                // --color or --color=WHEN
                if (wcscmp(a, L"--color") == 0 || wcscmp(a, L"--color=auto")==0) { opts.opt_color_auto = true; }
                else if (wcscmp(a, L"--color=always")==0) { opts.opt_color_always = true; }
                else if (wcscmp(a, L"--color=never")==0) { opts.opt_color_never = true; }
                else if (starts_with(a, L"--color=")) {
                    wchar_t *v = a + wcslen(L"--color=");
                    if (wcscmp(v, L"auto")==0) opts.opt_color_auto = true;
                    else if (wcscmp(v, L"always")==0) opts.opt_color_always = true;
                    else if (wcscmp(v, L"never")==0) opts.opt_color_never = true;
                } else opts.opt_color_auto = true;
                ++i; continue;
            }
            if (wcscmp(a, L"--si")==0) { opts.opt_si = true; ++i; continue; }
            if (wcscmp(a, L"--author")==0) { opts.opt_author = true; ++i; continue; }
            if (wcscmp(a, L"--full-time")==0) { wcscpy(opts.time_style, L"full-iso"); ++i; continue; }
            if (wcscmp(a, L"--hyperlink")==0 || starts_with(a, L"--hyperlink=")) { opts.opt_hyperlink = true; ++i; continue; }
            // unknown long option
            fwprintf(stderr, L"ls: unrecognized option '%s'\n", a);
            return 2;
        }
        // short options
        for (int j = 1; a[j]; ++j) {
            wchar_t c = a[j];
            switch (c) {
                case L'a': opts.opt_all = true; break;
                case L'A': opts.opt_almost_all = true; break;
                case L'l': opts.opt_long = true; break;
                case L'R': opts.opt_recursive = true; break;
                case L'h': opts.opt_human = true; break;
                case L'S': opts.sort = SORT_SIZE; break;
                case L't': opts.sort = SORT_TIME; break;
                case L'U': opts.opt_unsorted = true; opts.sort = SORT_NONE; break;
                case L'i': opts.opt_inode = true; break;
                case L'n': opts.opt_numeric_ids = true; break;
                case L'N': opts.opt_literal = true; break;
                case L'Q': opts.opt_quote = true; break;
                case L'q': opts.opt_hide_control = true; break;
                case L'L': opts.opt_dereference = true; break;
                case L'F': opts.opt_classify = true; break;
                case L'p': opts.opt_indicator_slash = true; break;
                case L'B': opts.opt_ignore_backups = true; break;
                case L'1': opts.opt_one = true; break;
                case L'm': opts.opt_comma = true; break;
                case L'f': opts.opt_all = true; opts.opt_unsorted = true; break;
                case L'g': /* like -l but no owner */ opts.opt_long = true; opts.opt_no_group = true; break;
                case L'o': /* like -l but no group */ opts.opt_long = true; break;
                case L'v': opts.sort = SORT_VERSION; break;
                case L'X': opts.sort = SORT_EXTENSION; break;
                case L'r': opts.opt_reverse = true; break;
                case L's': opts.opt_size_alloc = true; break;
                case L'c': /* time selection: ctime */ opts.time_kind = TIME_CTIME; break;
                case L'u': opts.time_kind = TIME_ATIME; break;
                case L'd': /* list directories themselves */ opts.opt_dired = true; break;
                default:
                    fwprintf(stderr, L"ls: invalid option -- '%c'\n", c);
                    return 2;
            }
        }
        ++i;
    }

    // remaining args are paths
    if (i < argc) {
        paths_given = true;
        paths_count = argc - i;
        paths = &argv[i];
    }

    // default color behavior: auto if not specified
    if (!opts.opt_color_always && !opts.opt_color_auto && !opts.opt_color_never) opts.opt_color_auto = true;

    // default time style if not set
    if (wcslen(opts.time_style) == 0) wcscpy(opts.time_style, L"");

    // if no paths, use "."
    if (!paths_given) {
        wchar_t cur[PATH_MAX];
        if (!GetCurrentDirectoryW(PATH_MAX, cur)) wcscpy(cur, L".");
        static wchar_t *default_paths[1];
        default_paths[0] = cur;
        paths = default_paths;
        paths_count = 1;
    }

    // process each path
    int exit_status = 0;
    bool multiple = (paths_count > 1);
    for (int pi = 0; pi < paths_count; ++pi) {
        const wchar_t *p = paths[pi];
        // if multiple, print header
        if (multiple) wprintf(L"%s:\n", p);
        // check attributes
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (!GetFileAttributesExW(p, GetFileExInfoStandard, &fad)) {
            fwprintf(stderr, L"ls: cannot access '%s'\n", p);
            exit_status = 2;
            continue;
        }
        if (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY && !opts.opt_dired) {
            int rc = list_dir_recursive(p, false);
            if (rc != 0) exit_status = rc;
        } else {
            // single file: print info
            Entry e; memset(&e, 0, sizeof(e));
            const wchar_t *name = p;
            const wchar_t *slash = wcsrchr(p, L'\\');
            if (!slash) slash = wcsrchr(p, L'/');
            if (slash) name = slash + 1;
            wcscpy(e.name, name);
            wcscpy(e.full, p);
            WIN32_FIND_DATAW fd;
            HANDLE h = FindFirstFileW(p, &fd);
            if (h == INVALID_HANDLE_VALUE) {
                fwprintf(stderr, L"ls: cannot stat '%s'\n", p);
                exit_status = 1;
            } else {
                e.fd = fd;
                e.size = ((unsigned long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
                e.times[0] = fd.ftLastWriteTime;
                e.times[1] = fd.ftLastAccessTime;
                e.times[2] = fd.ftCreationTime;
                FindClose(h);
                if (opts.opt_long) print_long_entry(&e);
                else print_short_entry(&e);
            }
        }
        if (pi + 1 < paths_count) wprintf(L"\n");
    }

    return exit_status;
}