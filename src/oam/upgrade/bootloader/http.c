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

struct upgrade_bootloader_http_t {
    struct http_server_t server;
    uint32_t application_address;
    uint32_t application_size;
    struct flash_driver_t *flash_p;
    struct {
        uint32_t address;
    } swdl;
};

/* Forward declarations. */
static int http_request_application_erase(struct http_server_connection_t *connection_p,
                                          struct http_server_request_t *request_p);
static int http_request_application_write(struct http_server_connection_t *connection_p,
                                          struct http_server_request_t *request_p);
static int http_request_application_sha1(struct http_server_connection_t *connection_p,
                                         struct http_server_request_t *request_p);

static struct upgrade_bootloader_http_t module;
static THRD_STACK(listener_stack, 2048);
static THRD_STACK(connection_stack, 4096);

static struct http_server_route_t routes[] = {
    { .path_p = "/oam/upgrade/application/erase",
      .callback = http_request_application_erase },
    { .path_p = "/oam/upgrade/application/write",
      .callback = http_request_application_write },
    { .path_p = "/oam/upgrade/application/sha1",
      .callback = http_request_application_sha1 },
    { .path_p = NULL, .callback = NULL }
};

static struct http_server_listener_t listener = {
    .address_p = STRINGIFY(CONFIG_UPGRADE_HTTP_SERVER_IP),
    .port = CONFIG_UPGRADE_HTTP_SERVER_PORT,
    .thrd = {
        .name_p = "http_listener",
        .stack = {
            .buf_p = listener_stack,
            .size = sizeof(listener_stack)
        }
    }
};

static struct http_server_connection_t connections[] = {
    {
        .thrd = {
            .name_p = "http_conn_0",
            .stack = {
                .buf_p = connection_stack,
                .size = sizeof(connection_stack)
            }
        }
    },
    {
        .thrd = {
            .name_p = NULL
        }
    }
};

/**
 * HTTP server request to erase the application area.
 *
 * @return zero(0) or negative error code.
 */
static int http_request_application_erase(struct http_server_connection_t *connection_p,
                                          struct http_server_request_t *request_p)
{
    struct http_server_response_t response;

    /* Only the GET action is supported. */
    if (request_p->action != http_server_request_action_get_t) {
        return (-1);
    }

    /* Create the response. */
    if (upgrade_bootloader_application_erase() != 0) {
        response.code = http_server_response_code_400_bad_request_t;
        response.content.buf_p = "erase failed";
    } else {
        response.code = http_server_response_code_200_ok_t;
        response.content.buf_p = "erase successful";
    }
    
    response.content.type = http_server_content_type_text_plain_t;
    response.content.size = strlen(response.content.buf_p);

    return (http_server_response_write(connection_p,
                                       request_p,
                                       &response));
}

/**
 * HTTP server request to upload an application image to the
 * application area.
 *
 * @return zero(0) or negative error code.
 */
static int http_request_application_write(struct http_server_connection_t *connection_p,
                                          struct http_server_request_t *request_p)
{
    struct http_server_response_t response;
    int res;
    char buf[512];
    size_t left;
    size_t size;

    /* Only the POST action is supported. */
    if (request_p->action != http_server_request_action_post_t) {
        return (-1);
    }

    /* Only accept application/octet-stream content. */
    if ((request_p->headers.content_type.present == 0) ||
        strcmp(&request_p->headers.content_type.value[0],
               "application/octet-stream") != 0) {
        return (-1);
    }

    /* Content length must be present. */
    if (request_p->headers.content_length.present == 0) {
        return (-1);
    }

    left = request_p->headers.content_length.value;

    /* Reply with "100 Continue" if expected by the client. */
    if (request_p->headers.expect.present == 1) {
        if (strcmp(&request_p->headers.expect.value[0], "100-continue") != 0) {
            return (-1);
        }

        if (chan_write(&connection_p->socket,
                       "HTTP/1.1 100 Continue\r\n\r\n",
                       29) != 29) {
            return (-1);
        }
    }

    /* Write received octet stream to the application area. */
    res = upgrade_bootloader_application_write_begin();

    if (res == 0) {
        while ((res == 0) && (left > 0)) {
            if (left > sizeof(buf)) {
                size = sizeof(buf);
            } else {
                size = left;
            }

            if (chan_read(&connection_p->socket, &buf[0], size) == size) {
                res = upgrade_bootloader_application_write_chunk(&buf[0], size);
                left -= size;
            } else {
                res = -1;
            }
        }

        if (upgrade_bootloader_application_write_end() != 0) {
            res = -1;
        }

        if (res == 0) {
            res = upgrade_bootloader_application_write_valid_flag();
        }
    }

    /* Create the response. */
    if (res == 0) {
        response.code = http_server_response_code_200_ok_t;
        response.content.buf_p = "write successful";
    } else {
        response.code = http_server_response_code_400_bad_request_t;
        response.content.buf_p = "write failed";
    }

    response.content.type = http_server_content_type_text_plain_t;
    response.content.size = strlen(response.content.buf_p);

    return (http_server_response_write(connection_p,
                                       request_p,
                                       &response));
}

/**
 * HTTP server request to calculate the SHA1 hash of the application
 * area.
 *
 * @return zero(0) or negative error code.
 */
static int http_request_application_sha1(struct http_server_connection_t *connection_p,
                                         struct http_server_request_t *request_p)
{
    struct http_server_response_t response;
    uint8_t sha1[20];
    char sha1hex[41];
    int i;

    if (upgrade_bootloader_application_sha1(&sha1[0]) == 0) {
        for (i = 0; i < membersof(sha1); i++) {
            std_sprintf(&sha1hex[2 * i], FSTR("%02x"), sha1[i]);
        }

        response.code = http_server_response_code_200_ok_t;
        response.content.buf_p = &sha1hex[0];
    } else {
        response.code = http_server_response_code_400_bad_request_t;
        response.content.buf_p = "sha1 failed";
    }

    response.content.type = http_server_content_type_text_plain_t;
    response.content.size = strlen(response.content.buf_p);

    return (http_server_response_write(connection_p,
                                       request_p,
                                       &response));
}

/**
 * Default page handler.
 */
static int no_route(struct http_server_connection_t *connection_p,
                    struct http_server_request_t *request_p)
{
    struct http_server_response_t response;
    
    /* Create the response. */
    response.code = http_server_response_code_404_not_found_t;
    response.content.type = http_server_content_type_text_plain_t;
    response.content.buf_p = NULL;
    response.content.size = 0;

    return (http_server_response_write(connection_p,
                                       request_p,
                                       &response));
}

int upgrade_bootloader_http_module_init()
{
    return (http_server_init(&module.server,
                             &listener,
                             connections,
                             NULL,
                             routes,
                             no_route));
}

int upgrade_bootloader_http_start()
{
    return (http_server_start(&module.server));
}
