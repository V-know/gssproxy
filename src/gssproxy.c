/*
   GSS-PROXY

   Copyright (C) 2011 Red Hat, Inc.
   Copyright (C) 2011 Simo Sorce <simo.sorce@redhat.com>

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/

#include "config.h"
#include <stdlib.h>
#include "popt.h"
#include "gp_utils.h"

int main(int argc, const char *argv[])
{
    int opt;
    poptContext pc;
    int opt_daemon = 0;
    int opt_interactive = 0;
    int opt_version = 0;
    char *opt_config_file = NULL;
    verto_ctx *vctx;
    verto_ev *ev;
    int vflags;
    int fd;
    struct gssproxy_ctx *gpctx;

    struct poptOption long_options[] = {
        POPT_AUTOHELP
        {"daemon", 'D', POPT_ARG_NONE, &opt_daemon, 0, \
         _("Become a daemon (default)"), NULL }, \
        {"interactive", 'i', POPT_ARG_NONE, &opt_interactive, 0, \
         _("Run interactive (not a daemon)"), NULL}, \
        {"config", 'c', POPT_ARG_STRING, &opt_config_file, 0, \
         _("Specify a non-default config file"), NULL}, \
         {"version", '\0', POPT_ARG_NONE, &opt_version, 0, \
          _("Print version number and exit"), NULL }, \
        POPT_TABLEEND
    };

    pc = poptGetContext(argv[0], argc, argv, long_options, 0);
    while((opt = poptGetNextOpt(pc)) != -1) {
        switch(opt) {
        default:
            fprintf(stderr, "\nInvalid option %s: %s\n\n",
                    poptBadOption(pc, 0), poptStrerror(opt));
            poptPrintUsage(pc, stderr, 0);
            return 1;
        }
    }

    if (opt_version) {
        puts(VERSION""DISTRO_VERSION""PRERELEASE_VERSION);
        return 0;
    }

    if (opt_daemon && opt_interactive) {
        fprintf(stderr, "Option -i|--interactive is not allowed together with -D|--daemon\n");
        poptPrintUsage(pc, stderr, 0);
        return 1;
    }

    if (opt_interactive) {
        opt_daemon = 2;
    }

    gpctx = calloc(1, sizeof(struct gssproxy_ctx));

    gpctx->config = read_config(opt_config_file, opt_daemon);
    if (!gpctx->config) {
        exit(EXIT_FAILURE);
    }

    init_server(gpctx->config->daemonize);

    fd = init_unix_socket(gpctx->config->socket_name);
    if (fd == -1) {
        return 1;
    }

    vctx = init_event_loop();
    if (!vctx) {
        return 1;
    }

    vflags = VERTO_EV_FLAG_PERSIST | VERTO_EV_FLAG_IO_READ;
    ev = verto_add_io(vctx, vflags, accept_sock_conn, fd);
    if (!ev) {
        return 1;
    }
    verto_set_private(ev, gpctx, NULL);

    gpctx->workers = gp_workers_init(vctx, gpctx->config);
    if (!gpctx->workers) {
        exit(EXIT_FAILURE);
    }

    verto_run(vctx);

    gp_workers_free(gpctx->workers);

    fini_server();

    poptFreeContext(pc);

    return 0;
}