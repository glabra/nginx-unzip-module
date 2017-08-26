/**
 * @file   ngx_http_unzip_module.c
 * @author Bartek Jarocki <bartek@youzee.com>
 * @date   Fri Jun 15 10:52:12 2012
 *
 * @brief  This module extract files from unzip archive on the fly
 *
 * @section LICENSE
 *
 * Copyright (C) 2012 by Youzee Media Entertainment SL
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <zip.h>

#define NGX_HTTP_UNZIP_NOCASE_DISABLE 0
#define NGX_HTTP_UNZIP_NOCASE_FALLBACK 1
#define NGX_HTTP_UNZIP_NOCASE_ALWAYS 2

static ngx_int_t ngx_http_unzip_init(ngx_conf_t *cf);
static ngx_int_t ngx_http_unzip_handler(ngx_http_request_t *r);

static ngx_conf_enum_t  ngx_http_unzip_nocase_type[] = {
    { ngx_string("disable"), NGX_HTTP_UNZIP_NOCASE_DISABLE},
    { ngx_string("fallback"), NGX_HTTP_UNZIP_NOCASE_FALLBACK},
    { ngx_string("always"), NGX_HTTP_UNZIP_NOCASE_ALWAYS},
    { ngx_null_string, 0 }
};

typedef struct {
    ngx_flag_t enable;
    ngx_http_complex_value_t *archive;
    ngx_http_complex_value_t *target;
    ngx_uint_t nocase;
    ngx_flag_t autoindex;
} ngx_http_unzip_loc_conf_t;

/**
 * This module let you keep your files inside zip archive file
 * and serve (unzipping on the fly) them as requested.
 */
static ngx_command_t ngx_http_unzip_commands[] = {
    {
      ngx_string("unzip"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_unzip_loc_conf_t, enable),
      NULL
    }, {
      ngx_string("unzip_archive"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_set_complex_value_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_unzip_loc_conf_t, archive),
      NULL
    }, {
      ngx_string("unzip_path"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_set_complex_value_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_unzip_loc_conf_t, target),
      NULL
    }, {
      ngx_string("unzip_nocase"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_unzip_loc_conf_t, nocase),
      &ngx_http_unzip_nocase_type
    }, {
      ngx_string("unzip_autoindex"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_unzip_loc_conf_t, autoindex),
      NULL
    },
    ngx_null_command
};

static void *
ngx_http_unzip_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_unzip_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_unzip_loc_conf_t));
    if (conf == NULL) {
        return NGX_CONF_ERROR;
    }

    conf->enable = NGX_CONF_UNSET;
    conf->nocase = NGX_CONF_UNSET_UINT;
    conf->autoindex = NGX_CONF_UNSET;

    return conf;
}

static char *
ngx_http_unzip_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_unzip_loc_conf_t *prev = parent;
    ngx_http_unzip_loc_conf_t *conf = child;

    if (conf->target == NULL) {
        conf->target = prev->target;
    }

    if (conf->archive == NULL) {
        conf->archive = prev->archive;
    }

    ngx_conf_merge_value(conf->autoindex, prev->autoindex, 0);
    ngx_conf_merge_uint_value(conf->nocase, prev->nocase, NGX_HTTP_UNZIP_NOCASE_DISABLE);
    ngx_conf_merge_value(conf->enable, prev->enable, 0);

    return NGX_CONF_OK;
}

/* The module context. */
static ngx_http_module_t ngx_http_unzip_module_ctx = {
    NULL, /* preconfiguration */
    ngx_http_unzip_init, /* postconfiguration */

    NULL, /* create main configuration */
    NULL, /* init main configuration */

    NULL, /* create server configuration */
    NULL, /* merge server configuration */

    ngx_http_unzip_create_loc_conf, /* create location configuration */
    ngx_http_unzip_merge_loc_conf /* merge location configuration */
};


/* Module definition. */
ngx_module_t ngx_http_unzip_module = {
    NGX_MODULE_V1,
    &ngx_http_unzip_module_ctx, /* module context */
    ngx_http_unzip_commands, /* module directives */
    NGX_HTTP_MODULE, /* module type */
    NULL, /* init master */
    NULL, /* init module */
    NULL, /* init process */
    NULL, /* init thread */
    NULL, /* exit thread */
    NULL, /* exit process */
    NULL, /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_buf_t *
ngx_http_unzip_autoindex(ngx_http_request_t *r, struct zip *archive, const char *target)
{
    ngx_buf_t *b;
    struct zip_stat st;
    zip_int64_t total;
    ngx_array_t entries;
    ngx_str_t *entry;
    size_t target_len, st_len, html_len;

    static u_char header[] =
        "<!DOCTYPE html><html><body><h1>Index</h1>"
        "<hr><ul><li><a href=\"../\">../</a></li>";

    static u_char footer[] =
        "</ul><hr></body></html>";

    total = zip_get_num_entries(archive, 0);
    if (total < 0) {
        return NULL;
    }

    if (ngx_array_init(&entries, r->pool, 50, sizeof(ngx_str_t)) != NGX_OK) {
        return NULL;
    }

    target_len = ngx_strlen(target);
    for (zip_int64_t i = 0; i < total; i++) {
        zip_stat_index(archive, i, 0, &st);

        if (ngx_strncmp(st.name, target, target_len) != 0) {
            continue;
        }

        st_len = ngx_strlen(st.name);
        if (st_len <= target_len) {
            continue;
        }

        const char *path = st.name + target_len;
        size_t path_len = st_len - target_len;

        char *p = ngx_strchr(path, '/');
        if (p) {
            path_len -= ngx_strlen(p + 1);
        }

        /* TODO: more efficient duplicate check */
        ngx_int_t matched = 0;
        entry = entries.elts;
        for (ngx_uint_t i = 0; i < entries.nelts; i++) {
            if (ngx_strncmp(path, entry[i].data, entry[i].len) == 0) {
                matched = 1;
                break;
            }
        }

        if (matched) {
            continue;
        }

        entry = ngx_array_push(&entries);
        if (!entry) {
            return NULL;
        }

        entry->len = path_len;
        entry->data = ngx_pnalloc(r->pool, entry->len);
        ngx_memcpy(entry->data, (u_char *)path, entry->len);
    }

    html_len = sizeof(header) - 1
             + sizeof(footer) - 1;

    entry = entries.elts;
    for (ngx_uint_t i = 0; i < entries.nelts; i++) {
        html_len += sizeof("<li><a href=\"\"></a></li>") - 1
                  + entry[i].len * 2;
    }

    b = ngx_create_temp_buf(r->pool, html_len);
    if (!b) {
        return NULL;
    }

    b->last = ngx_copy(b->last, header, sizeof(header) - 1);
    entry = entries.elts;
    for (ngx_uint_t i = 0; i < entries.nelts; i++) {
        b->last = ngx_copy(b->last, "<li><a href=\"", sizeof("<li><a href=\"") - 1);
        b->last = ngx_copy(b->last, entry[i].data, entry[i].len);
        b->last = ngx_copy(b->last, "\">", sizeof("\">") - 1);
        b->last = ngx_copy(b->last, entry[i].data, entry[i].len);
        b->last = ngx_copy(b->last, "</a></li>", sizeof("</a></li>") - 1);
    }
    b->last = ngx_copy(b->last, footer, sizeof(footer) - 1);

    return b;
}

static ngx_buf_t *
ngx_http_unzip_inflate(ngx_http_request_t *r, struct zip *archive, const char *target)
{
    ngx_buf_t   *b;
    struct      zip_stat st;
    struct      zip_file *file;
    unsigned char *content;
    zip_int64_t index;

    ngx_http_unzip_loc_conf_t *conf;
    conf = ngx_http_get_module_loc_conf(r, ngx_http_unzip_module);

    switch (conf->nocase) {
    case NGX_HTTP_UNZIP_NOCASE_FALLBACK:
        index = zip_name_locate(archive, target, 0);
        if (index < 0) {
            index = zip_name_locate(archive, target, ZIP_FL_NOCASE);
        }
        break;
    case NGX_HTTP_UNZIP_NOCASE_ALWAYS:
        index = zip_name_locate(archive, target, ZIP_FL_NOCASE);
        break;
    default: /* NGX_HTTP_UNZIP_NOCASE_DISABLE */
        index = zip_name_locate(archive, target, 0);
        break;
    }

    if (0 != zip_stat_index(archive, index, 0, &st)) {
        return NULL;
    }

    if (!(content = ngx_palloc(r->pool, st.size))) {
        return NULL;
    }

    if (!(file = zip_fopen_index(archive, index, 0))) {
        return NULL;
    }

    if (zip_fread(file, content, st.size) != (zip_int64_t)st.size) {
        zip_fclose(file);
        ngx_pfree(r->pool, content);
        return NULL;
    }

    zip_fclose(file);

    /* allocate a new buffer for sending out the reply. */
    b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
    if (!b) {
        return NULL;
    }

    b->pos = content;
    b->last = content + st.size;

    return b;
}

static ngx_int_t
ngx_http_unzip_handler(ngx_http_request_t *r)
{
    ngx_chain_t out;
    ngx_buf_t  *buf;
    ngx_str_t   unzip_archive_str;
    ngx_str_t   unzip_target_str;
    struct      zip *zip_source;
    char        *unzip_archive;
    char        *unzip_target;

    ngx_http_unzip_loc_conf_t *conf;
    conf = ngx_http_get_module_loc_conf(r, ngx_http_unzip_module);

    /* pass if archive_path or target_path not defined */
    if (!conf->enable || !conf->target || !conf->archive) {
        return NGX_DECLINED;
    }

    /* let's try to get file_in_unzip_archivefile and file_in_unzip_extract from nginx configuration */
    if (ngx_http_complex_value(r, conf->archive, &unzip_archive_str) != NGX_OK
            || ngx_http_complex_value(r, conf->target, &unzip_target_str) != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Failed to read unzip module configuration settings.");
        return NGX_ERROR;
    }

    /* we're supporting just GET and HEAD requests */
    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Only GET and HEAD requests are supported by the unzip module.");
        return NGX_HTTP_NOT_ALLOWED;
    }

    /* fill path variables with 0 as ngx_string_t doesn't terminate string with 0 */
    unzip_archive = (char *)ngx_palloc(r->pool, unzip_archive_str.len + 1);
    unzip_target = (char *)ngx_palloc(r->pool, unzip_target_str.len + 1);
    if (!unzip_archive || !unzip_target) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Failed to allocate buffer.");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    /* get path variables terminated with 0 */
    ngx_memcpy(unzip_archive, (char *)unzip_archive_str.data, unzip_archive_str.len);
    ngx_memcpy(unzip_target, (char *)unzip_target_str.data, unzip_target_str.len);
    unzip_archive[unzip_archive_str.len] = '\0';
    unzip_target[unzip_target_str.len] = '\0';

    /* try to open archive (zip) file */
    if (!(zip_source = zip_open(unzip_archive, ZIP_CHECKCONS|ZIP_RDONLY, NULL))) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "%s : no such archive.", unzip_archive);
        return NGX_HTTP_NOT_FOUND;
    }

    ngx_uint_t status = NGX_OK;
    if (unzip_target_str.data[unzip_target_str.len - 1] == '/' || unzip_target_str.len == 0) {
        /* directory listing when uri ends with '/' */
        if (conf->autoindex) {
            buf = ngx_http_unzip_autoindex(r, zip_source, unzip_target);
            ngx_str_set(&r->headers_out.content_type, "text/html");
            if (!buf) {
                status = NGX_ERROR;
            }
        } else {
            status = NGX_HTTP_NOT_FOUND;
        }
    } else {
        buf = ngx_http_unzip_inflate(r, zip_source, unzip_target);
        if (!buf) {
            status = NGX_HTTP_NOT_FOUND;
        }
    }

    zip_close(zip_source);

    if (status != NGX_OK) {
        return status;
    }

    buf->memory = 1;
    buf->last_buf = 1;
    buf->last_in_chain = 1;

    out.buf = buf;
    out.next = NULL; /* just one buffer */

    /* sending the headers for the reply. */
    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = buf->last - buf->pos;
    ngx_http_send_header(r);

    return ngx_http_output_filter(r, &out);
}

static ngx_int_t
ngx_http_unzip_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt        *h;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
    h = ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers);
    if (!h) {
        return NGX_ERROR;
    }

    *h = ngx_http_unzip_handler;

    return NGX_OK;
}

