/*
** Copyright (C) 2012 Fargier Sylvain <fargier.sylvain@free.fr>
**
** This software is provided 'as-is', without any express or implied
** warranty.  In no event will the authors be held liable for any damages
** arising from the use of this software.
**
** Permission is granted to anyone to use this software for any purpose,
** including commercial applications, and to alter it and redistribute it
** freely, subject to the following restrictions:
**
** 1. The origin of this software must not be misrepresented; you must not
**    claim that you wrote the original software. If you use this software
**    in a product, an acknowledgment in the product documentation would be
**    appreciated but is not required.
** 2. Altered source versions must be plainly marked as such, and must not be
**    misrepresented as being the original software.
** 3. This notice may not be removed or altered from any source distribution.
**
** redirect.c
**
**        Created on: Jun 01, 2012
**   Original Author: Fargier Sylvain <fargier.sylvain@free.fr>
**
*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#endif

#ifndef __linux__
#error redirect is currently Linux-only.
#endif

void _debug(const char *pfx, const char *msg, va_list ap) {

    if (pfx)
        fprintf(stderr, "%s", pfx);
    vfprintf(stderr, msg, ap);
    fprintf(stderr, "\n");
}

void die(const char *msg, ...) {
    va_list ap;
    va_start(ap, msg);
    _debug("[!] ", msg, ap);
    va_end(ap);

    exit(1);
}

void debug(const char *msg, ...) {
    va_list ap;
    va_start(ap, msg);
    _debug("[+] ", msg, ap);
    va_end(ap);
}

void error(const char *msg, ...) {
    va_list ap;
    va_start(ap, msg);
    _debug("[-] ", msg, ap);
    va_end(ap);
}

int parse_pid(const char *pidarg, pid_t *pid)
{
    char *endptr = NULL;
    *pid = strtol(pidarg, &endptr, 10);
    if (!endptr || (*endptr != '\0'))
        return -1;
    return 0;
}

void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [options] [pid [file]]\n", prog);
    fputs("\nOptions:\n", stderr);
    fputs("  -h, --help           Show this help message and exit\n", stderr);
    fputs("  -p, --pid            PID of the process to redirect\n", stderr);
    fputs("  -o, --out            stdin/stderr output file\n", stderr);
}

int main(int argc, char **argv)
{
    pid_t pid = 0;
    const char *log_file = NULL;

#ifdef HAVE_GETOPT_LONG
    int c;
    struct option long_options[] = {
        { "help", 0, 0, 'h' },
        { "pid", 1, 0, 'p' },
        { "out", 1, 0, 'o' },
        { 0, 0, 0, 0 }
    };

    while ((c = getopt_long(argc, argv, "hp:o:", long_options, NULL)) != -1)
    {
        switch (c)
        {
            case 'h':
                usage(argv[0]);
                exit(EXIT_SUCCESS);
            case 'p':
                if (parse_pid(optarg, &pid) != 0)
                    die("invalid pid argument");
                break;
            case 'o':
                log_file = optarg;
                break;
        }
    }
#else
    int optind = 1;
#endif

    if (argc > optind)
    {
        if (parse_pid(argv[optind], &pid) != 0)
            die("invalid pid argument");
        ++optind;
    }
    if (argc > optind)
        log_file = argv[optind];

    if (pid == 0)
        die("PID missing");

    if (!log_file)
        die("output file missing");

    return redirect_child(pid, log_file);
}


