// rm.c
// Windows port of rm (C). Moves to Recycle Bin by default; -p for permanent deletion.
// Supports: -f, -i, -I, --interactive=WHEN, -r/-R, -d, -v, -p/--permanent,
// --one-file-system, --preserve-root, --no-preserve-root, --help, --version.
//
// Notes:
// - Uses SHFileOperationW for recycle bin moves (FOF_ALLOWUNDO).
// - For recursive deletion, uses FindFirstFileW/FindNextFileW.
// - One-file-system compares volume serial numbers via GetVolumePathNameW + GetVolumeInformationW.
// - Preserve-root treats drive roots (e.g., "C:\") as protected unless --no-preserve-root is used.
// - Attempts to prompt for unwritable files unless -f is given.
// - This is a best-effort port; Windows semantics differ from Unix in many edge cases.

#define _UNICODE
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <stdio.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>
#include <tchar.h>
#include <io.h>
#include <sys/stat.h>

#define VERSION L"Windows rm port by penguincre8te (1.0)"

typedef enum { INTER_NEVER, INTER_ONCE, INTER_ALWAYS } interactive_mode_t;

typedef struct {
    int force;
    int verbose;
    int permanent;
    int recursive;
    int remove_empty_dir;
    int preserve_root; // default true
    int no_preserve_root;
    int one_file_system;
    interactive_mode_t interactive;
} options_t;

static options_t opts = {0,0,0,0,0,1,0,0, INTER_NEVER};
static int stdin_is_tty = 0;
static int prompt_once_done = 0;
static int prompt_once_answer = 0;

// Utility: print usage
static void print_help(const wchar_t *prog) {
    wprintf(L"Usage: %s [OPTION]... [FILE]...\n", prog);
    wprintf(L"Remove (unlink) the FILE(s). By default files are moved to the Recycle Bin.\n\n");
    wprintf(L"  -f, --force            ignore nonexistent files and arguments, never prompt\n");
    wprintf(L"  -i                     prompt before every removal\n");
    wprintf(L"  -I                     prompt once before removing more than three files, or when removing recursively\n");
    wprintf(L"  --interactive[=WHEN]   prompt according to WHEN: never, once (-I), or always (-i)\n");
    wprintf(L"  -r, -R, --recursive    remove directories and their contents recursively\n");
    wprintf(L"  -d, --dir              remove empty directories\n");
    wprintf(L"  -p, --permanent        permanently remove files instead of moving to Recycle Bin\n");
    wprintf(L"  -v, --verbose          explain what is being done\n");
    wprintf(L"  --one-file-system      when removing recursively, skip directories on other volumes\n");
    wprintf(L"  --preserve-root[=all]  do not remove drive root (default)\n");
    wprintf(L"  --no-preserve-root     do not treat drive root specially\n");
    wprintf(L"  --help                 display this help and exit\n");
    wprintf(L"  --version              output version information and exit\n");
}

// Utility: print version
static void print_version(void) {
    wprintf(L"%s\n", VERSION);
}

// Check if path is drive root like "C:\"
static int is_drive_root(const wchar_t *path) {
    // Normalize: GetVolumePathNameW returns root path for a path
    wchar_t root[MAX_PATH];
    if (!GetVolumePathNameW(path, root, MAX_PATH)) return 0;
    // Compare path normalized to root
    // Remove trailing backslash differences
    size_t plen = wcslen(path);
    size_t rlen = wcslen(root);
    // Compare case-insensitive
    if (_wcsicmp(path, root) == 0) return 1;
    // Also handle when user passed "C:" (no backslash)
    wchar_t alt[MAX_PATH];
    if (plen == 2 && path[1] == L':') {
        // append backslash
        swprintf(alt, MAX_PATH, L"%s\\", path);
        if (_wcsicmp(alt, root) == 0) return 1;
    }
    return 0;
}

// Get volume serial number for path; returns 0 on failure
static DWORD get_volume_serial(const wchar_t *path) {
    wchar_t volRoot[MAX_PATH];
    if (!GetVolumePathNameW(path, volRoot, MAX_PATH)) return 0;
    DWORD serial = 0;
    if (!GetVolumeInformationW(volRoot, NULL, 0, &serial, NULL, NULL, NULL, 0)) return 0;
    return serial;
}

// Prompt helper: returns 1 for yes, 0 for no
static int prompt_yes_no(const wchar_t *fmt, ...) {
    if (!stdin_is_tty) return 0;
    va_list ap;
    va_start(ap, fmt);
    wchar_t buf[1024];
    vswprintf(buf, 1024, fmt, ap);
    va_end(ap);
    while (1) {
        wprintf(L"%s [y/N]: ", buf);
        fflush(stdout);
        wchar_t line[16];
        if (!fgetws(line, 16, stdin)) return 0;
        // trim
        wchar_t c = towlower(line[0]);
        if (c == L'y') return 1;
        if (c == L'n' || c == L'\n' || c == L'\r' || c == 0) return 0;
    }
}

// Remove a single file or empty directory permanently
static int delete_permanent(const wchar_t *path, int verbose, const wchar_t *prog) {
    DWORD attrs = GetFileAttributesW(path);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        if (!opts.force) fwprintf(stderr, L"%s: cannot remove '%s': No such file or directory\n", prog, path);
        return 0;
    }
    if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
        // directory
        if (RemoveDirectoryW(path)) {
            if (verbose) wprintf(L"removed directory '%s'\n", path);
            return 1;
        } else {
            if (!opts.force) fwprintf(stderr, L"%s: failed to remove directory '%s' (error %lu)\n", prog, path, GetLastError());
            return 0;
        }
    } else {
        // file: remove read-only attribute if necessary
        if (attrs & FILE_ATTRIBUTE_READONLY) {
            SetFileAttributesW(path, attrs & ~FILE_ATTRIBUTE_READONLY);
        }
        if (DeleteFileW(path)) {
            if (verbose) wprintf(L"removed '%s'\n", path);
            return 1;
        } else {
            if (!opts.force) fwprintf(stderr, L"%s: failed to remove '%s' (error %lu)\n", prog, path, GetLastError());
            return 0;
        }
    }
}

// Move to Recycle Bin using SHFileOperationW
static int move_to_recycle(const wchar_t *path, int verbose, const wchar_t *prog) {
    // SHFILEOPSTRUCT requires double-null terminated string
    size_t len = wcslen(path);
    wchar_t *from = (wchar_t*)malloc((len + 2) * sizeof(wchar_t));
    if (!from) return 0;
    wcscpy(from, path);
    from[len+1] = L'\0';
    from[len] = L'\0';
    SHFILEOPSTRUCTW op = {0};
    op.hwnd = NULL;
    op.wFunc = FO_DELETE;
    op.pFrom = from;
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI;
    // If interactive prompting is desired, we will have already asked the user.
    int res = SHFileOperationW(&op);
    free(from);
    if (res == 0) {
        if (verbose) wprintf(L"moved '%s' to Recycle Bin\n", path);
        return 1;
    } else {
        if (!opts.force) fwprintf(stderr, L"%s: failed to move '%s' to Recycle Bin (SHFileOperation error %d)\n", prog, path, res);
        return 0;
    }
}

// Check if path is "." or ".." last component
static int is_dot_or_dotdot(const wchar_t *path) {
    // Find last component
    const wchar_t *p = path + wcslen(path);
    while (p > path && (*(p-1) == L'\\' || *(p-1) == L'/')) p--;
    const wchar_t *start = p;
    while (start > path && *(start-1) != L'\\' && *(start-1) != L'/') start--;
    size_t len = p - start;
    if (len == 1 && start[0] == L'.') return 1;
    if (len == 2 && start[0] == L'.' && start[1] == L'.') return 1;
    return 0;
}

// Forward declaration
static int remove_path_recursive(const wchar_t *path, DWORD parent_vol_serial, const wchar_t *prog);

// Remove a path (file or directory). Returns 1 on success, 0 on failure.
static int remove_path(const wchar_t *path, const wchar_t *prog) {
    if (is_dot_or_dotdot(path)) {
        fwprintf(stderr, L"%s: refusing to remove '.' or '..' component: '%s'\n", prog, path);
        return 0;
    }

    DWORD attrs = GetFileAttributesW(path);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        if (!opts.force) fwprintf(stderr, L"%s: cannot remove '%s': No such file or directory\n", prog, path);
        return 0;
    }

    // Preserve-root check: treat drive root specially
    if (opts.preserve_root && !opts.no_preserve_root) {
        if (is_drive_root(path)) {
            fwprintf(stderr, L"%s: it is dangerous to operate recursively on '%s'\n", prog, path);
            fwprintf(stderr, L"Use --no-preserve-root to override this failsafe.\n");
            return 0;
        }
    }

    // Interactive logic
    if (opts.interactive == INTER_ALWAYS) {
        if (!prompt_yes_no(L"remove '%s'?", path)) return 0;
    } else if (opts.interactive == INTER_ONCE) {
        if (!prompt_once_done) {
            // For single-file calls, INTER_ONCE behaves like INTER_ALWAYS only if more than 3 files or recursive.
            // The caller sets prompt_once_done/prompt_once_answer based on context; here we just check.
            // If prompt_once_done is not set, default to not prompting.
        }
    } else {
        // If file is unwritable and stdin is a terminal and not forced, prompt
        if (!opts.force && stdin_is_tty && !(opts.interactive == INTER_ALWAYS)) {
            // Check write permission: attempt to open for write
            HANDLE h = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                   NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (h == INVALID_HANDLE_VALUE) {
                // unwritable or locked; prompt
                if (!prompt_yes_no(L"remove write-protected '%s'?", path)) return 0;
            } else {
                CloseHandle(h);
            }
        }
    }

    if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
        // Directory
        if (!opts.recursive) {
            // If -d specified, remove only if empty
            if (opts.remove_empty_dir) {
                // Try RemoveDirectory
                if (opts.permanent) {
                    if (RemoveDirectoryW(path)) {
                        if (opts.verbose) wprintf(L"removed directory '%s'\n", path);
                        return 1;
                    } else {
                        if (!opts.force) fwprintf(stderr, L"winrm: failed to remove directory '%s' (error %lu)\n", path, GetLastError());
                        return 0;
                    }
                } else {
                    // Move empty directory to recycle bin
                    if (move_to_recycle(path, opts.verbose, prog)) return 1;
                    return 0;
                }
            } else {
                fwprintf(stderr, L"winrm: cannot remove '%s': Is a directory\n", path);
                return 0;
            }
        } else {
            // recursive removal
            DWORD vol_serial = 0;
            if (opts.one_file_system) vol_serial = get_volume_serial(path);
            return remove_path_recursive(path, vol_serial, prog);
        }
    } else {
        // File
        if (opts.permanent) {
            return delete_permanent(path, opts.verbose, prog);
        } else {
            return move_to_recycle(path, opts.verbose, prog);
        }
    }
}

// Helper to join two paths safely into buffer
static void join_path(const wchar_t *base, const wchar_t *name, wchar_t *out, size_t outlen) {
    if (wcslen(base) == 0) {
        wcsncpy(out, name, outlen-1);
        out[outlen-1] = L'\0';
        return;
    }
    wcscpy(out, base);
    size_t len = wcslen(out);
    if (len > 0 && out[len-1] != L'\\' && out[len-1] != L'/') {
        if (len + 1 < outlen) {
            out[len] = L'\\';
            out[len+1] = L'\0';
        }
    }
    wcsncat(out, name, outlen - wcslen(out) - 1);
}

// Recursive removal implementation
static int remove_path_recursive(const wchar_t *path, DWORD parent_vol_serial, const wchar_t *prog) {
    // If one-file-system is set, check volume serial
    if (opts.one_file_system) {
        DWORD vol = get_volume_serial(path);
        if (parent_vol_serial != 0 && vol != parent_vol_serial) {
            if (opts.verbose) wprintf(L"skipping '%s' (different file system)\n", path);
            return 1; // skip but treat as success
        }
    }

    // Enumerate directory contents
    wchar_t search[MAX_PATH];
    join_path(path, L"*", search, MAX_PATH);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(search, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        // Could be empty or inaccessible
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND) {
            // empty directory
        } else {
            if (!opts.force) fwprintf(stderr, L"%s: cannot access '%s' (error %lu)\n", prog, path, err);
            return 0;
        }
    } else {
        do {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
            wchar_t child[MAX_PATH];
            join_path(path, fd.cFileName, child, MAX_PATH);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                // Recurse
                if (!remove_path_recursive(child, parent_vol_serial, prog)) {
                    // continue trying others unless force is not set
                    if (!opts.force) {
                        FindClose(h);
                        return 0;
                    }
                }
            } else {
                // File
                if (opts.interactive == INTER_ALWAYS) {
                    if (!prompt_yes_no(L"remove '%s'?", child)) continue;
                } else if (opts.interactive == INTER_ONCE) {
                    if (!prompt_once_done) {
                        // handled by caller; if not set, default to not prompting
                    }
                } else {
                    // unwritable check
                    if (!opts.force && stdin_is_tty) {
                        HANDLE fh = CreateFileW(child, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                        if (fh == INVALID_HANDLE_VALUE) {
                            if (!prompt_yes_no(L"remove write-protected '%s'?", child)) continue;
                        } else CloseHandle(fh);
                    }
                }
                if (opts.permanent) {
                    delete_permanent(child, opts.verbose, prog);
                } else {
                    move_to_recycle(child, opts.verbose, prog);
                }
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }

    // After contents removed, remove the directory itself
    if (opts.permanent) {
        if (RemoveDirectoryW(path)) {
            if (opts.verbose) wprintf(L"removed directory '%s'\n", path);
            return 1;
        } else {
            if (!opts.force) fwprintf(stderr, L"%s: failed to remove directory '%s' (error %lu)\n", prog, path, GetLastError());
            return 0;
        }
    } else {
        // Move directory to recycle bin
        if (move_to_recycle(path, opts.verbose, prog)) return 1;
        return 0;
    }
}

// Count targets to decide -I behavior
static int count_targets(int argc, wchar_t **argv, int start) {
    int c = 0;
    for (int i = start; i < argc; ++i) {
        if (argv[i][0] == L'-') continue;
        c++;
    }
    return c;
}

// Parse interactive WHEN string
static int parse_interactive_when(const wchar_t *s, interactive_mode_t *out) {
    if (!s) { *out = INTER_ALWAYS; return 1; } // --interactive with no arg => always
    if (_wcsicmp(s, L"never") == 0) { *out = INTER_NEVER; return 1; }
    if (_wcsicmp(s, L"once") == 0) { *out = INTER_ONCE; return 1; }
    if (_wcsicmp(s, L"always") == 0) { *out = INTER_ALWAYS; return 1; }
    return 0;
}

int wmain(int argc, wchar_t **argv) {
    // Basic init
    stdin_is_tty = _isatty(_fileno(stdin));
    // Default options
    opts.force = 0;
    opts.verbose = 0;
    opts.permanent = 0;
    opts.recursive = 0;
    opts.remove_empty_dir = 0;
    opts.preserve_root = 1;
    opts.no_preserve_root = 0;
    opts.one_file_system = 0;
    opts.interactive = INTER_NEVER;

    // Parse args (simple)
    int i = 1;
    int show_help = 0;
    int show_version = 0;
    // We'll collect non-option args into an array
    wchar_t **targets = (wchar_t**)malloc(sizeof(wchar_t*) * (argc+1));
    int tcount = 0;

    while (i < argc) {
        wchar_t *arg = argv[i];
        if (arg[0] != L'-' || wcscmp(arg, L"-") == 0) {
            targets[tcount++] = arg;
            i++;
            continue;
        }
        // Long options
        if (wcscmp(arg, L"--help") == 0) { show_help = 1; i++; continue; }
        if (wcscmp(arg, L"--version") == 0) { show_version = 1; i++; continue; }
        if (wcscmp(arg, L"--force") == 0) { opts.force = 1; i++; continue; }
        if (wcscmp(arg, L"--permanent") == 0) { opts.permanent = 1; i++; continue; }
        if (wcscmp(arg, L"--recursive") == 0) { opts.recursive = 1; i++; continue; }
        if (wcscmp(arg, L"--dir") == 0) { opts.remove_empty_dir = 1; i++; continue; }
        if (wcscmp(arg, L"--verbose") == 0) { opts.verbose = 1; i++; continue; }
        if (wcscmp(arg, L"--one-file-system") == 0) { opts.one_file_system = 1; i++; continue; }
        if (wcsncmp(arg, L"--preserve-root", 14) == 0) { opts.preserve_root = 1; i++; continue; }
        if (wcscmp(arg, L"--no-preserve-root") == 0) { opts.no_preserve_root = 1; opts.preserve_root = 0; i++; continue; }
        if (wcsncmp(arg, L"--interactive", 13) == 0) {
            // Could be --interactive or --interactive=WHEN
            wchar_t *eq = wcschr(arg, L'=');
            if (eq) {
                if (!parse_interactive_when(eq+1, &opts.interactive)) {
                    fwprintf(stderr, L"%s: invalid argument to --interactive: %s\n", argv[0], eq+1);
                    return 2;
                }
            } else {
                // If next arg exists and doesn't start with '-', treat as WHEN
                if (i+1 < argc && argv[i+1][0] != L'-') {
                    if (!parse_interactive_when(argv[i+1], &opts.interactive)) {
                        fwprintf(stderr, L"%s: invalid argument to --interactive: %s\n", argv[0], argv[i+1]);
                        return 2;
                    }
                    i++;
                } else {
                    opts.interactive = INTER_ALWAYS;
                }
            }
            i++; continue;
        }
        // Short options or combined
        if (arg[1] == L'-') {
            // unknown long option
            fwprintf(stderr, L"%s: unrecognized option '%s'\n", argv[0], arg);
            return 2;
        }
        // iterate characters after '-'
        for (int p = 1; arg[p] != L'\0'; ++p) {
            wchar_t c = arg[p];
            switch (c) {
                case L'f': opts.force = 1; break;
                case L'i': opts.interactive = INTER_ALWAYS; break;
                case L'I': opts.interactive = INTER_ONCE; break;
                case L'r': opts.recursive = 1; break;
                case L'R': opts.recursive = 1; break;
                case L'd': opts.remove_empty_dir = 1; break;
                case L'v': opts.verbose = 1; break;
                case L'p': opts.permanent = 1; break;
                default:
                    fwprintf(stderr, L"%s: invalid option -- '%c'\n", argv[0], c);
                    return 2;
            }
        }
        i++;
    }

    if (show_help) { print_help(argv[0]); return 0; }
    if (show_version) { print_version(); return 0; }

    if (tcount == 0) {
        fwprintf(stderr, L"%s: missing operand\n", argv[0]);
        fwprintf(stderr, L"Try '%s --help' for more information.\n", argv[0]);
        return 2;
    }

    // Decide INTER_ONCE behavior: if more than 3 targets or recursive present, prompt once
    if (opts.interactive == INTER_ONCE) {
        int cnt = tcount;
        if (cnt > 3 || opts.recursive) {
            if (stdin_is_tty) {
                prompt_once_done = 1;
                prompt_once_answer = prompt_yes_no(L"Remove all %d arguments?", cnt);
                if (!prompt_once_answer) {
                    wprintf(L"Aborted.\n");
                    return 0;
                }
            } else {
                // non-interactive stdin: treat as not allowed
                if (!opts.force) {
                    fwprintf(stderr, L"%s: interactive prompt required but stdin is not a terminal\n", argv[0]);
                    return 2;
                }
            }
        } else {
            // behave like INTER_NEVER for small number of files
            opts.interactive = INTER_NEVER;
        }
    }

    // For each target, attempt removal
    int exit_status = 0;
    DWORD parent_vol_serial = 0;
    if (opts.one_file_system && tcount > 0) {
        parent_vol_serial = get_volume_serial(targets[0]);
    }

    for (int j = 0; j < tcount; ++j) {
        wchar_t *t = targets[j];
        if (!remove_path(t, argv[0])) {
            exit_status = 1;
            if (!opts.force) {
                // continue to next but record failure
            }
        }
    }

    free(targets);
    return exit_status;
}