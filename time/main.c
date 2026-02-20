/*
  time_win.c
  A POSIX-like 'time' utility for Windows (best-effort).
  Supports: --help, --version, -p, -f/--format, -o/--output, -a, -v, --
  Build: cl /O2 /W4 time_win.c /link psapi.lib
         gcc -O2 -Wall -municode time_win.c -lpsapi -o time.exe
*/

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdarg.h>
#include <time.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#endif

#define VERSION "time-win 1.0"

static void print_help(void) {
    printf("Synopsis\n");
    printf("  time [options] command [arguments...]\n\n");
    printf("Options\n");
    printf("  -p                 Use portable output format\n");
    printf("  -f FORMAT, --format=FORMAT\n");
    printf("                     Use FORMAT for output (overrides TIME env)\n");
    printf("  -o FILE, --output=FILE\n");
    printf("                     Write results to FILE instead of stderr\n");
    printf("  -a, --append       Append to output file (with -o)\n");
    printf("  -v, --verbose      Verbose output\n");
    printf("  --help             Print this message and exit\n");
    printf("  -V, --version      Print version and exit\n");
    printf("  --                 End of options\n");
}

static void print_version(void) {
    printf("%s\n", VERSION);
}

/* Utility: join argv elements into a single command line (quotes as needed) */
static char *build_command_line(int argc, char **argv) {
    /* Estimate size */
    size_t needed = 1;
    for (int i = 0; i < argc; ++i) {
        needed += strlen(argv[i]) + 3;
    }
    char *cmd = (char*)malloc(needed);
    if (!cmd) return NULL;
    cmd[0] = '\0';
    for (int i = 0; i < argc; ++i) {
        const char *arg = argv[i];
        bool need_quote = strchr(arg, ' ') || strchr(arg, '\t') || arg[0] == '\0';
        if (i) strcat(cmd, " ");
        if (need_quote) {
            strcat(cmd, "\"");
            /* naive quoting: escape internal quotes by backslash */
            for (const char *p = arg; *p; ++p) {
                if (*p == '"') strcat(cmd, "\\\"");
                else {
                    size_t len = strlen(cmd);
                    cmd[len] = *p;
                    cmd[len+1] = '\0';
                }
            }
            strcat(cmd, "\"");
        } else {
            strcat(cmd, arg);
        }
    }
    return cmd;
}

/* High-resolution elapsed timer */
typedef struct {
    LARGE_INTEGER freq;
    LARGE_INTEGER start;
} hr_timer;

static void hr_timer_start(hr_timer *t) {
    QueryPerformanceFrequency(&t->freq);
    QueryPerformanceCounter(&t->start);
}

static double hr_timer_elapsed_seconds(hr_timer *t) {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    LONGLONG diff = now.QuadPart - t->start.QuadPart;
    return (double)diff / (double)t->freq.QuadPart;
}

/* Convert FILETIME to seconds (double) */
static double filetime_to_seconds(const FILETIME *ft) {
    /* FILETIME is 100-ns intervals since 1601 */
    ULARGE_INTEGER ui;
    ui.LowPart = ft->dwLowDateTime;
    ui.HighPart = ft->dwHighDateTime;
    return (double)ui.QuadPart / 1e7;
}

/* Format elapsed as H:MM:SS or M:SS or S */
static void format_elapsed_hms(double seconds, char *buf, size_t bufsz) {
    if (seconds < 0) seconds = 0;
    int hrs = (int)(seconds / 3600.0);
    int mins = (int)((seconds - hrs*3600) / 60.0);
    double secs = seconds - hrs*3600 - mins*60;
    if (hrs > 0) {
        snprintf(buf, bufsz, "%d:%02d:%06.3f", hrs, mins, secs);
    } else {
        snprintf(buf, bufsz, "%d:%06.3f", mins, secs);
    }
}

/* Minimal format engine: supports a subset of conversions */
static void format_output(const char *fmt, FILE *out,
                          double elapsed, double user_s, double sys_s,
                          SIZE_T max_rss_kb, SIZE_T avg_rss_kb,
                          SIZE_T data_kb, SIZE_T stack_kb, SIZE_T text_kb,
                          ULONGLONG io_in, ULONGLONG io_out,
                          DWORD page_faults, DWORD major_faults,
                          int exit_code, const char *cmdline)
{
    const char *p = fmt;
    while (*p) {
        if (*p == '%') {
            ++p;
            if (*p == '%') { fputc('%', out); ++p; continue; }
            char conv = *p++;
            switch (conv) {
                case 'e': /* elapsed seconds */
                    fprintf(out, "%.3f", elapsed);
                    break;
                case 'E': { /* elapsed H:MM:SS */
                    char buf[64];
                    format_elapsed_hms(elapsed, buf, sizeof(buf));
                    fprintf(out, "%s", buf);
                    break;
                }
                case 'U': /* user seconds */
                    fprintf(out, "%.3f", user_s);
                    break;
                case 'S': /* system seconds */
                    fprintf(out, "%.3f", sys_s);
                    break;
                case 'P': { /* percentage CPU */
                    double pct = (elapsed > 0.0) ? ((user_s + sys_s) / elapsed * 100.0) : 0.0;
                    fprintf(out, "%.0f%%", pct);
                    break;
                }
                case 'M': /* max resident set size (KB) */
                    fprintf(out, "%zu", (size_t)max_rss_kb);
                    break;
                case 't': /* average resident set size (KB) */
                    fprintf(out, "%zu", (size_t)avg_rss_kb);
                    break;
                case 'K': /* average total memory (KB) */
                    fprintf(out, "%zu", (size_t)avg_rss_kb);
                    break;
                case 'D': /* average data size (KB) */
                    fprintf(out, "%zu", (size_t)data_kb);
                    break;
                case 'p': /* average stack size (KB) */
                    fprintf(out, "%zu", (size_t)stack_kb);
                    break;
                case 'X': /* average shared text (KB) */
                    fprintf(out, "%zu", (size_t)text_kb);
                    break;
                case 'Z': /* page size in bytes */
                    fprintf(out, "%u", (unsigned)GetSystemInfo, 0); /* placeholder */
                    break;
                case 'F': /* major page faults */
                    fprintf(out, "%u", major_faults);
                    break;
                case 'R': /* minor page faults */
                    fprintf(out, "%u", page_faults);
                    break;
                case 'W': /* swaps */
                    fprintf(out, "%d", 0);
                    break;
                case 'c': /* involuntary context switches */
                    fprintf(out, "%d", 0);
                    break;
                case 'w': /* voluntary context switches */
                    fprintf(out, "%d", 0);
                    break;
                case 'I': /* inputs */
                    fprintf(out, "%" PRIu64, io_in);
                    break;
                case 'O': /* outputs */
                    fprintf(out, "%" PRIu64, io_out);
                    break;
                case 'r': /* socket messages received */
                    fprintf(out, "%d", 0);
                    break;
                case 's': /* socket messages sent */
                    fprintf(out, "%d", 0);
                    break;
                case 'k': /* signals delivered */
                    fprintf(out, "%d", 0);
                    break;
                case 'C': /* command */
                    fprintf(out, "%s", cmdline ? cmdline : "");
                    break;
                case 'x': /* exit status */
                    fprintf(out, "%d", exit_code);
                    break;
                default:
                    /* Unknown conversion: print it literally */
                    fputc('%', out);
                    fputc(conv, out);
                    break;
            }
        } else if (*p == '\\') {
            ++p;
            if (*p == 'n') { fputc('\n', out); ++p; }
            else if (*p == 't') { fputc('\t', out); ++p; }
            else if (*p == '\\') { fputc('\\', out); ++p; }
            else { fputc('\\', out); }
        } else {
            fputc(*p++, out);
        }
    }
    /* ensure trailing newline */
    fputc('\n', out);
}

int main(int argc, char **argv) {
    bool portable = false;
    bool verbose = false;
    char *format = NULL;
    char *outfile = NULL;
    bool append = false;
    int argi = 1;
    bool endopts = false;

    /* Default format: try TIME env, else a reasonable default */
    char *env_time = getenv("TIME");
    const char *default_format = "%U user %S system %E elapsed %P CPU (%X text+%D data %M max)k\n%I inputs+%O outputs (%F major + %R minor) pagefaults %W swaps";

    while (argi < argc) {
        char *a = argv[argi];
        if (!endopts && a[0] == '-') {
            if (strcmp(a, "--") == 0) { endopts = true; argi++; continue; }
            if (strcmp(a, "--help") == 0) { print_help(); return 0; }
            if (strcmp(a, "--version") == 0 || strcmp(a, "-V") == 0) { print_version(); return 0; }
            if (strcmp(a, "-p") == 0 || strcmp(a, "--portability") == 0) { portable = true; argi++; continue; }
            if (strcmp(a, "-v") == 0 || strcmp(a, "--verbose") == 0) { verbose = true; argi++; continue; }
            if (strcmp(a, "-a") == 0 || strcmp(a, "--append") == 0) { append = true; argi++; continue; }
            if (strncmp(a, "--format=", 9) == 0) { format = _strdup(a + 9); argi++; continue; }
            if (strcmp(a, "-f") == 0) {
                if (argi + 1 >= argc) { fprintf(stderr, "Missing argument for -f\n"); return 1; }
                format = _strdup(argv[++argi]); argi++; continue;
            }
            if (strncmp(a, "--output=", 9) == 0) { outfile = _strdup(a + 9); argi++; continue; }
            if (strcmp(a, "-o") == 0) {
                if (argi + 1 >= argc) { fprintf(stderr, "Missing argument for -o\n"); return 1; }
                outfile = _strdup(argv[++argi]); argi++; continue;
            }
            /* unknown option */
            fprintf(stderr, "Unknown option: %s\n", a);
            return 1;
        } else break;
    }

    if (argi >= argc) {
        fprintf(stderr, "No command specified. Use --help for usage.\n");
        return 1;
    }

    /* Determine format */
    if (portable) {
        format = strdup("real %e\nuser %U\nsys %S\n");
    } else if (!format) {
        if (env_time) format = _strdup(env_time);
        else format = _strdup(default_format);
    }

    /* Build command line from remaining args */
    int cmdargc = argc - argi;
    char **cmdargv = &argv[argi];
    char *cmdline = build_command_line(cmdargc, cmdargv);
    if (!cmdline) { fprintf(stderr, "Out of memory\n"); return 1; }

    /* Prepare output FILE* */
    FILE *out = stderr;
    if (outfile) {
        out = fopen(outfile, append ? "a" : "w");
        if (!out) {
            fprintf(stderr, "Cannot open output file '%s' for writing\n", outfile);
            /* continue to stderr */
            out = stderr;
        }
    }

    /* CreateProcess */
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    /* Start timer */
    hr_timer t;
    hr_timer_start(&t);

    BOOL created = CreateProcessA(
        NULL,               /* lpApplicationName */
        cmdline,            /* lpCommandLine */
        NULL,               /* lpProcessAttributes */
        NULL,               /* lpThreadAttributes */
        FALSE,              /* bInheritHandles */
        CREATE_NEW_CONSOLE, /* dwCreationFlags - create console so GUI apps run */
        NULL,               /* lpEnvironment */
        NULL,               /* lpCurrentDirectory */
        &si,
        &pi
    );

    if (!created) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            fprintf(stderr, "Command not found: %s\n", cmdline);
            return 127;
        } else if (err == ERROR_ACCESS_DENIED) {
            fprintf(stderr, "Permission denied: %s\n", cmdline);
            return 126;
        } else {
            fprintf(stderr, "CreateProcess failed (error %lu)\n", err);
            return 1;
        }
    }

    /* Wait for process to finish */
    DWORD wait = WaitForSingleObject(pi.hProcess, INFINITE);
    (void)wait;

    /* Stop timer */
    double elapsed = hr_timer_elapsed_seconds(&t);

    /* Get exit code */
    DWORD exit_code = 0;
    if (!GetExitCodeProcess(pi.hProcess, &exit_code)) {
        exit_code = 1;
    }

    /* Get process times */
    FILETIME ftCreation, ftExit, ftKernel, ftUser;
    ZeroMemory(&ftCreation, sizeof(ftCreation));
    ZeroMemory(&ftExit, sizeof(ftExit));
    ZeroMemory(&ftKernel, sizeof(ftKernel));
    ZeroMemory(&ftUser, sizeof(ftUser));
    if (!GetProcessTimes(pi.hProcess, &ftCreation, &ftExit, &ftKernel, &ftUser)) {
        /* failed */
    }
    double user_s = filetime_to_seconds(&ftUser);
    double sys_s  = filetime_to_seconds(&ftKernel);

    /* Get IO counters */
    IO_COUNTERS ioc;
    ZeroMemory(&ioc, sizeof(ioc));
    ULONGLONG io_in = 0, io_out = 0;
    if (GetProcessIoCounters(pi.hProcess, &ioc)) {
        io_in = ioc.ReadOperationCount;
        io_out = ioc.WriteOperationCount;
    }

    /* Get memory info */
    PROCESS_MEMORY_COUNTERS pmc;
    ZeroMemory(&pmc, sizeof(pmc));
    SIZE_T max_rss_kb = 0;
    SIZE_T avg_rss_kb = 0;
    SIZE_T data_kb = 0, stack_kb = 0, text_kb = 0;
    DWORD page_faults = 0;
    if (GetProcessMemoryInfo(pi.hProcess, &pmc, sizeof(pmc))) {
        max_rss_kb = pmc.PeakWorkingSetSize / 1024;
        avg_rss_kb = pmc.WorkingSetSize / 1024;
        page_faults = pmc.PageFaultCount;
        /* Windows does not expose data/text/stack sizes in the same way */
        data_kb = 0; stack_kb = 0; text_kb = 0;
    }

    /* Major/minor page faults: Windows doesn't split them; set major=0, minor=PageFaultCount */
    DWORD major_faults = 0;
    DWORD minor_faults = page_faults;

    /* Verbose output */
    if (verbose) {
        fprintf(out, "Command: %s\n", cmdline);
        fprintf(out, "Elapsed: %.6f s\n", elapsed);
        fprintf(out, "User: %.6f s\n", user_s);
        fprintf(out, "System: %.6f s\n", sys_s);
        fprintf(out, "Peak RSS: %zu KB\n", (size_t)max_rss_kb);
        fprintf(out, "Page faults: %u\n", page_faults);
    }

    /* Format and print results */
    format_output(format, out, elapsed, user_s, sys_s,
                  max_rss_kb, avg_rss_kb, data_kb, stack_kb, text_kb,
                  io_in, io_out, minor_faults, major_faults, (int)exit_code, cmdline);

    /* Cleanup */
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    free(cmdline);
    free(format);
    if (outfile && out != stderr) fclose(out);

    /* Return child's exit code */
    return (int)exit_code;
}