/**
 * @section License
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2014-2016, Erik Moqvist
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * This file is part of the Simba project.
 */

#include "simba.h"

struct upgrade_bootloader_tftp_t {
    struct tftp_server_t server;
};

/* Forward declarations. */
static int file_open(struct fs_filesystem_t *filesystem_p,
                     struct fs_file_t *self_p,
                     const char *path_p,
                     int flags);
static int file_close(struct fs_file_t *self_p);
static ssize_t file_write(struct fs_file_t *self_p,
                          const void *src_p,
                          size_t size);

static struct upgrade_bootloader_tftp_t module;
static THRD_STACK(tftp_server_stack, 2048);

static struct fs_filesystem_operations_t tftp_ops = {
    .file_open = file_open,
    .file_close = file_close,
    .file_write = file_write
};
static struct fs_filesystem_t fs;

/* File system callbacks. */
static int file_open(struct fs_filesystem_t *filesystem_p,
                     struct fs_file_t *self_p,
                     const char *path_p,
                     int flags)
{
    if ((flags & FS_WRITE) == 0) {
        return (-1);
    }

    return (upgrade_bootloader_application_write_begin());
}

static int file_close(struct fs_file_t *self_p)
{
    return (upgrade_bootloader_application_write_end());
}

static ssize_t file_write(struct fs_file_t *self_p,
                          const void *src_p,
                          size_t size)
{
    int res;

    res = upgrade_bootloader_application_write_chunk(src_p, size);

    return (res == 0 ? size : -1);
}

int upgrade_bootloader_tftp_module_init()
{
    struct inet_addr_t addr;
    const char *root_p = "/tftp";

    if (inet_aton(STRINGIFY(CONFIG_UPGRADE_TFTP_SERVER_IP), &addr.ip) != 0) {
        return (-1);
    }

    addr.port = CONFIG_UPGRADE_TFTP_SERVER_PORT;

    /* Create and register the tftp file system used to write received
       software to the application area. */
    fs_filesystem_init_generic(&fs, root_p, &tftp_ops);
    fs_filesystem_register(&fs);

    return (tftp_server_init(&module.server,
                             &addr,
                             CONFIG_UPGRADE_TFTP_SERVER_TIMEOUT_MS,
                             "tftp_server",
                             root_p,
                             tftp_server_stack,
                             sizeof(tftp_server_stack)));
}

int upgrade_bootloader_tftp_start()
{
    return (tftp_server_start(&module.server));
}
