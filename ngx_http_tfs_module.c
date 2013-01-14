
/*
 * Copyright (C) 2010-2012 Alibaba Group Holding Limited
 */


#include <ngx_core.h>
#include <ngx_event.h>
#include <ngx_http.h>
#include <ngx_config.h>
#include <ngx_http_tfs.h>
#include <ngx_http_tfs_timers.h>
#include <ngx_http_tfs_local_block_cache.h>


#define NGX_HTTP_TFS_RCS_ZONE_NAME "tfs_module_rcs_zone"
#define NGX_HTTP_TFS_BLOCK_CACHE_ZONE_NAME "tfs_module_block_cache_zone"

static void *ngx_http_tfs_create_main_conf(ngx_conf_t *cf);
static char *ngx_http_tfs_init_main_conf(ngx_conf_t *cf, void *conf);

static void *ngx_http_tfs_create_srv_conf(ngx_conf_t *cf);
static char *ngx_http_tfs_merge_srv_conf(ngx_conf_t *cf,
    void *parent, void *child);

static void *ngx_http_tfs_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_tfs_merge_loc_conf(ngx_conf_t *cf,
    void *parent, void *child);

static ngx_int_t ngx_http_tfs_module_init(ngx_cycle_t *cycle);

static char *ngx_http_tfs_upstream(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_http_tfs_pass(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

static char *ngx_http_tfs_tackle_log(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

static char *ngx_http_tfs_net_device(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

static char *ngx_http_tfs_lowat_check(ngx_conf_t *cf, void *post, void *data);
static void ngx_http_tfs_read_body_handler(ngx_http_request_t *r);
static char *ngx_http_tfs_keepalive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static char *ngx_http_tfs_poll_rcs(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static char *ngx_http_tfs_rcs_zone(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_http_tfs_block_cache_zone(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

/* rc server keepalive */
static ngx_int_t ngx_http_tfs_check_init_worker(ngx_cycle_t *cycle);
#ifdef NGX_HTTP_TFS_USE_TAIR
/* destroy tair servers */
static void ngx_http_tfs_check_exit_worker(ngx_cycle_t *cycle);
#endif


static ngx_conf_post_t  ngx_http_tfs_lowat_post =
    { ngx_http_tfs_lowat_check };


static ngx_command_t  ngx_http_tfs_commands[] = {

    { ngx_string("tfs_upstream"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_http_tfs_upstream,
      0,
      0,
      NULL },

    { ngx_string("tfs_pass"),
      NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE12,
      ngx_http_tfs_pass,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("tfs_net_device"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_http_tfs_net_device,
      NGX_HTTP_SRV_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("tfs_keepalive"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE2,
      ngx_http_tfs_keepalive,
      0,
      0,
      NULL },

    { ngx_string("tfs_poll_rcs"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE3,
      ngx_http_tfs_poll_rcs,
      0,
      0,
      NULL },

    { ngx_string("tfs_tackle_log"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE12,
      ngx_http_tfs_tackle_log,
      NGX_HTTP_SRV_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("tfs_connect_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      0,
      offsetof(ngx_http_tfs_main_conf_t, tfs_connect_timeout),
      NULL },

    { ngx_string("tfs_send_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      0,
      offsetof(ngx_http_tfs_main_conf_t, tfs_send_timeout),
      NULL },

    { ngx_string("tfs_read_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      0,
      offsetof(ngx_http_tfs_main_conf_t, tfs_read_timeout),
      NULL },

    { ngx_string("tair_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      0,
      offsetof(ngx_http_tfs_main_conf_t, tair_timeout),
      NULL },

    { ngx_string("tfs_send_lowat"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      0,
      offsetof(ngx_http_tfs_main_conf_t, send_lowat),
      &ngx_http_tfs_lowat_post },

    { ngx_string("tfs_buffer_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      0,
      offsetof(ngx_http_tfs_main_conf_t, buffer_size),
      NULL },

    { ngx_string("tfs_body_buffer_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      0,
      offsetof(ngx_http_tfs_main_conf_t, body_buffer_size),
      NULL },

    { ngx_string("tfs_rcs_zone"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_1MORE,
      ngx_http_tfs_rcs_zone,
      0,
      0,
      NULL },

    { ngx_string("tfs_block_cache_zone"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_1MORE,
      ngx_http_tfs_block_cache_zone,
      0,
      0,
      NULL },

    { ngx_string("tfs_enable_remote_block_cache"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      0,
      offsetof(ngx_http_tfs_main_conf_t, enable_remote_block_cache),
      NULL },

    { ngx_string("tfs_enable_rcs"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      0,
      offsetof(ngx_http_tfs_main_conf_t, enable_rcs),
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_tfs_module_ctx = {
    NULL,                                  /* preconfiguration */
    NULL,                                  /* postconfiguration */

    ngx_http_tfs_create_main_conf,         /* create main configuration */
    ngx_http_tfs_init_main_conf,           /* init main configuration */

    ngx_http_tfs_create_srv_conf,          /* create server configuration */
    ngx_http_tfs_merge_srv_conf,           /* merge server configuration */

    ngx_http_tfs_create_loc_conf,          /* create location configuration */
    ngx_http_tfs_merge_loc_conf            /* merge location configuration */
};


ngx_module_t  ngx_http_tfs_module = {
    NGX_MODULE_V1,
    &ngx_http_tfs_module_ctx,              /* module context */
    ngx_http_tfs_commands,                 /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    ngx_http_tfs_module_init,              /* init module */
    ngx_http_tfs_check_init_worker,        /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
#ifdef NGX_HTTP_TFS_USE_TAIR
    ngx_http_tfs_check_exit_worker,        /* exit process */
#else
    NULL,
#endif
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_int_t
ngx_http_tfs_handler(ngx_http_request_t *r)
{
    ngx_int_t                       rc;
    ngx_http_tfs_t                 *t;
    ngx_http_tfs_loc_conf_t        *tlcf;
    ngx_http_tfs_srv_conf_t        *tscf;
    ngx_http_tfs_main_conf_t       *tmcf;

    tlcf = ngx_http_get_module_loc_conf(r, ngx_http_tfs_module);
    tscf = ngx_http_get_module_srv_conf(r, ngx_http_tfs_module);
    tmcf = ngx_http_get_module_main_conf(r, ngx_http_tfs_module);

    t = ngx_pcalloc(r->pool, sizeof(ngx_http_tfs_t));

    if (t == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "alloc ngx_http_tfs_t failed");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    t->pool = r->pool;
    t->data = r;
    t->log = r->connection->log;
    t->loc_conf = tlcf;
    t->srv_conf = tscf;
    t->main_conf = tmcf;
    t->output.tag = (ngx_buf_tag_t) &ngx_http_tfs_module;
    if (tmcf->local_block_cache_ctx != NULL) {
        t->block_cache_ctx.use_cache |= NGX_HTTP_TFS_LOCAL_BLOCK_CACHE;
        t->block_cache_ctx.local_ctx = tmcf->local_block_cache_ctx;
    }
    if (tmcf->enable_remote_block_cache == NGX_HTTP_TFS_YES) {
        t->block_cache_ctx.use_cache |= NGX_HTTP_TFS_REMOTE_BLOCK_CACHE;
    }
    t->block_cache_ctx.remote_ctx.data = t;
    t->block_cache_ctx.remote_ctx.tair_instance = &tmcf->remote_block_cache_instance;
    t->block_cache_ctx.curr_lookup_cache = NGX_HTTP_TFS_LOCAL_BLOCK_CACHE;

    rc = ngx_http_restful_parse(r, &t->r_ctx);
    if (rc != NGX_OK) {
        return rc;
    }

    t->header_only = r->header_only;

    if (!tmcf->enable_rcs && t->r_ctx.version == 2) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "custom file requires tfs_enable_rcs on,"
                      " and make sure you have MetaServer and RootServer!");
        return NGX_HTTP_BAD_REQUEST;
    }

    switch (t->r_ctx.action.code) {
    case NGX_HTTP_TFS_ACTION_CREATE_DIR:
    case NGX_HTTP_TFS_ACTION_CREATE_FILE:
    case NGX_HTTP_TFS_ACTION_REMOVE_DIR:
    case NGX_HTTP_TFS_ACTION_REMOVE_FILE:
    case NGX_HTTP_TFS_ACTION_MOVE_DIR:
    case NGX_HTTP_TFS_ACTION_MOVE_FILE:
    case NGX_HTTP_TFS_ACTION_LS_DIR:
    case NGX_HTTP_TFS_ACTION_LS_FILE:
    case NGX_HTTP_TFS_ACTION_STAT_FILE:
    case NGX_HTTP_TFS_ACTION_KEEPALIVE:
    case NGX_HTTP_TFS_ACTION_READ_FILE:
    case NGX_HTTP_TFS_ACTION_GET_APPID:
        rc = ngx_http_discard_request_body(r);

        if (rc != NGX_OK) {
            return rc;
        }

        r->headers_out.content_length_n = -1;
        ngx_http_set_ctx(r, t, ngx_http_tfs_module);
        r->main->count++;
        ngx_http_tfs_read_body_handler(r);
        break;
    case NGX_HTTP_TFS_ACTION_WRITE_FILE:
        r->headers_out.content_length_n = -1;
        ngx_http_set_ctx(r, t, ngx_http_tfs_module);
        rc = ngx_http_read_client_request_body(r, ngx_http_tfs_read_body_handler);
        if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
            return rc;
        }
        break;
    }

    return NGX_DONE;
}


static char *
ngx_http_tfs_upstream(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_tfs_main_conf_t       *tmcf = conf;

    ngx_url_t                       u;
    ngx_str_t                      *value, *server_addr;

    value = cf->args->elts;
    server_addr = &value[1];

    ngx_memzero(&u, sizeof(ngx_url_t));

    u.url.len = server_addr->len;
    u.url.data = server_addr->data;

    if (ngx_parse_url(cf->pool, &u) != NGX_OK) {
        if (u.err) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                "%s in tfs \"%V\"", u.err, &u.url);
        }

        return NGX_CONF_ERROR;
    }

    tmcf->ups_addr = u.addrs;

    return NGX_CONF_OK;
}


static char *
ngx_http_tfs_keepalive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_tfs_main_conf_t  *tmcf = conf;

    ngx_int_t                       max_cached, bucket_count;
    ngx_str_t                      *value, s;
    ngx_uint_t                      i;
    ngx_http_connection_pool_t     *p;

    value = cf->args->elts;
    max_cached = 0;
    bucket_count = 0;

    for (i = 1; i < cf->args->nelts; i++) {
        if (ngx_strncmp(value[i].data, "max_cached=", 11) == 0) {

            s.len = value[i].len - 11;
            s.data = value[i].data + 11;

            max_cached = ngx_atoi(s.data, s.len);

            if (max_cached == NGX_ERROR || max_cached == 0) {
                goto invalid;
            }
            continue;
        }

        if (ngx_strncmp(value[i].data, "bucket_count=", 13) == 0) {

            s.len = value[i].len - 13;
            s.data = value[i].data + 13;

            bucket_count = ngx_atoi(s.data, s.len);
            if (bucket_count == NGX_ERROR || bucket_count == 0) {
                goto invalid;
            }
            continue;
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid parameter \"%V\"", &value[i]);
        return NGX_CONF_ERROR;
    }

    p = ngx_http_connection_pool_init(cf->pool, max_cached, bucket_count);
    if (p == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "connection pool init failed");
        return NGX_CONF_ERROR;
    }

    tmcf->conn_pool = p;
    return NGX_CONF_OK;

invalid:
    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid value \"%V\" in \"%V\" directive",
                       &value[i], &cmd->name);
    return NGX_CONF_ERROR;
}


static char *
ngx_http_tfs_poll_rcs(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_tfs_main_conf_t  *tmcf = conf;

    ngx_msec_t               interval;
    ngx_str_t               *value, s;
    ngx_uint_t               i;
    ngx_int_t                rcs_kp_enable;

    value = cf->args->elts;

    for (i = 1; i < cf->args->nelts; i++) {
        if (ngx_strncmp(value[i].data, "lock_file=", 10) == 0) {
            s.data = value[i].data + 10;
            s.len = value[i].len - 10;

            if (ngx_conf_full_name(cf->cycle, &s, 0) != NGX_OK) {
                goto rcs_timers_error;
            }

            tmcf->lock_file = s;
            continue;
        }

        if (ngx_strncmp(value[i].data, "interval=", 9) == 0) {
            s.data = value[i].data + 9;
            s.len = value[i].len - 9;

            interval = ngx_parse_time(&s, 0);

            if (interval == (ngx_msec_t) NGX_ERROR) {
                return "invalid value";
            }

            tmcf->rcs_interval = interval;
            continue;
        }

        if (ngx_strncmp(value[i].data, "enable=", 7) == 0) {
            s.data = value[i].data + 7;
            s.len = value[i].len - 7;

            rcs_kp_enable = ngx_atoi(s.data, s.len);
            if (rcs_kp_enable == NGX_ERROR) {
                goto rcs_timers_error;
            }

            tmcf->rcs_kp_enable = rcs_kp_enable;
            continue;
        }

        goto rcs_timers_error;
    }

    if (tmcf->lock_file.len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "tfs_poll directive must have lock file");
        return NGX_CONF_ERROR;
    }

    if (tmcf->rcs_interval < NGX_HTTP_TFS_MIN_TIMER_DELAY) {
        tmcf->rcs_interval = NGX_HTTP_TFS_MIN_TIMER_DELAY;
        ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                           "tfs_poll interval is small, so reset this value to 1000");
    }

    return NGX_CONF_OK;

rcs_timers_error:
    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid value \"%V\" in \"%V\" directive",
                       &value[i], &cmd->name);
    return NGX_CONF_ERROR;
}


static char *
ngx_http_tfs_net_device(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_tfs_srv_conf_t  *tscf = conf;

    ngx_int_t                       rc;
    ngx_str_t                      *value;

    value = cf->args->elts;
    rc = ngx_http_tfs_get_local_ip(value[1], &tscf->local_addr);
    if (rc == NGX_ERROR) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "device is invalid(%V)", &value[1]);
        return NGX_CONF_ERROR;
    }

    ngx_inet_ntop(AF_INET, &tscf->local_addr.sin_addr, tscf->local_addr_text, NGX_INET_ADDRSTRLEN);
    return NGX_CONF_OK;
}


static char *
ngx_http_tfs_tackle_log(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_tfs_srv_conf_t                *tscf = conf;
    ngx_str_t                              *value;

    if (tscf->log != NULL) {
        return "is duplicate";
    }

    value = cf->args->elts;

    tscf->log = ngx_log_create(cf->cycle, &value[1]);
    if (tscf->log == NULL) {
        return NGX_CONF_ERROR;
    }

    if (cf->args->nelts == 2) {
        tscf->log->log_level = NGX_LOG_INFO;
    }

    return ngx_log_set_levels(cf, tscf->log);
}


static void *
ngx_http_tfs_create_srv_conf(ngx_conf_t *cf)
{
    ngx_http_tfs_srv_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_tfs_srv_conf_t));
    if (conf == NULL) {
        return NGX_CONF_ERROR;
    }

    return conf;
}


static char *
ngx_http_tfs_merge_srv_conf(ngx_conf_t *cf, void *parent, void *child)
{
    return NGX_CONF_OK;
}


static void *
ngx_http_tfs_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_tfs_main_conf_t           *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_tfs_main_conf_t));
    if (conf == NULL) {
        return NGX_CONF_ERROR;
    }

    conf->tfs_connect_timeout = NGX_CONF_UNSET_MSEC;
    conf->tfs_send_timeout = NGX_CONF_UNSET_MSEC;
    conf->tfs_read_timeout = NGX_CONF_UNSET_MSEC;

    conf->tair_timeout = NGX_CONF_UNSET_MSEC;

    conf->send_lowat = NGX_CONF_UNSET_SIZE;
    conf->buffer_size = NGX_CONF_UNSET_SIZE;
    conf->body_buffer_size = NGX_CONF_UNSET_SIZE;

    conf->rcs_interval = NGX_CONF_UNSET;
    conf->conn_pool = NGX_CONF_UNSET_PTR;

    conf->enable_remote_block_cache = NGX_CONF_UNSET;
    conf->enable_rcs = NGX_CONF_UNSET;

    return conf;
}


static char *
ngx_http_tfs_init_main_conf(ngx_conf_t *cf, void *conf)
{
    ngx_http_tfs_main_conf_t *tmcf = conf;

    if (tmcf->tfs_connect_timeout == NGX_CONF_UNSET_MSEC) {
        tmcf->tfs_connect_timeout = 3000;
    }

    if (tmcf->tfs_send_timeout == NGX_CONF_UNSET_MSEC) {
        tmcf->tfs_send_timeout = 3000;
    }

    if (tmcf->tfs_read_timeout == NGX_CONF_UNSET_MSEC) {
        tmcf->tfs_read_timeout = 3000;
    }

    if (tmcf->tair_timeout == NGX_CONF_UNSET_MSEC) {
        tmcf->tair_timeout = 3000;
    }

    if (tmcf->send_lowat == NGX_CONF_UNSET_SIZE) {
        tmcf->send_lowat = 0;
    }

    if (tmcf->buffer_size == NGX_CONF_UNSET_SIZE) {
        tmcf->buffer_size = (size_t) ngx_pagesize / 2;
    }

    if (tmcf->body_buffer_size == NGX_CONF_UNSET_SIZE) {
        tmcf->body_buffer_size = (size_t) ngx_pagesize * 2;
    }

    if (tmcf->enable_rcs == NGX_CONF_UNSET) {
        tmcf->enable_rcs = NGX_HTTP_TFS_NO;
    }

    return NGX_CONF_OK;
}


static void *
ngx_http_tfs_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_tfs_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_tfs_loc_conf_t));
    if (conf == NULL) {
        return NGX_CONF_ERROR;
    }

    return conf;
}


static char *ngx_http_tfs_merge_loc_conf(ngx_conf_t *cf,
    void *parent, void *child)
{
    return NGX_CONF_OK;
}


static char *
ngx_http_tfs_pass(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_int_t                       enable;
    ngx_str_t                      *value, s;
    ngx_http_tfs_main_conf_t       *tmcf;
    ngx_http_tfs_srv_conf_t        *tscf;
    ngx_http_core_loc_conf_t       *clcf;

    value = cf->args->elts;
    if (ngx_strncmp(value[1].data, "enable=", 7) == 0) {
        s.len = value[1].len - 7;
        s.data = value[1].data + 7;

        enable = ngx_atoi(s.data, s.len);
        if (!enable) {
            return NGX_CONF_OK;
        }
    }

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    tscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_tfs_module);
    tmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_tfs_module);

    clcf->handler = ngx_http_tfs_handler;

    if (clcf->name.data[clcf->name.len - 1] == '/') {
        clcf->auto_redirect = 1;
    }

    if (tmcf->enable_rcs == NGX_CONF_UNSET) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "in tfs module must assign enable rcs, use directives \"tfs_enable_rcs\" ");
        return NGX_CONF_ERROR;
    }

    if (tmcf->enable_rcs) {
        if (tscf->local_addr_text[0] == '\0') {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "in tfs module must assign net device name, use directives \"tfs_net_device\" ");
            return NGX_CONF_ERROR;
        }

        s.data = (u_char *) NGX_HTTP_TFS_RCS_ZONE_NAME;
        s.len = sizeof(NGX_HTTP_TFS_RCS_ZONE_NAME) - 1;

        tmcf->rcs_shm_zone = ngx_shared_memory_add(cf, &s, 0,
                                                  &ngx_http_tfs_module);
        if (tmcf->rcs_shm_zone == NULL) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "in tfs module must assign rcs shm zone, use directives \"tfs_rcs_zone\" ");
            return NGX_CONF_ERROR;
        }
    }

    if (tmcf->local_block_cache_ctx != NULL) {
        s.data = (u_char *) NGX_HTTP_TFS_BLOCK_CACHE_ZONE_NAME;
        s.len = sizeof(NGX_HTTP_TFS_BLOCK_CACHE_ZONE_NAME) - 1;

        tmcf->block_cache_shm_zone = ngx_shared_memory_add(cf, &s, 0,
                                                  &ngx_http_tfs_module);
        if (tmcf->block_cache_shm_zone == NULL) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "in tfs module must assign block cache shm zone,"\
                               "use directives \"tfs_block_cache_zone\" ");
            return NGX_CONF_ERROR;
        }
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_tfs_lowat_check(ngx_conf_t *cf, void *post, void *data)
{
#if (NGX_FREEBSD)
    ssize_t *np = data;

    if ((u_long) *np >= ngx_freebsd_net_inet_tcp_sendspace) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"tfs_send_lowat\" must be less than %d "
                           "(sysctl net.inet.tcp.sendspace)",
                           ngx_freebsd_net_inet_tcp_sendspace);

        return NGX_CONF_ERROR;
    }

#elif !(NGX_HAVE_SO_SNDLOWAT)
    ssize_t *np = data;

    ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                       "\"tfs_send_lowat\" is not supported, ignored");

    *np = 0;

#endif

    return NGX_CONF_OK;
}


static char *
ngx_http_tfs_rcs_zone(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_tfs_main_conf_t        *tmcf = conf;
    size_t                           size;
    ngx_str_t                       *value, s, name;
    ngx_uint_t                       i;
    ngx_shm_zone_t                  *shm_zone;
    ngx_http_tfs_rc_ctx_t           *ctx;

    value = cf->args->elts;
    size = 0;

    for (i = 1; i < cf->args->nelts; i++) {

        if (ngx_strncmp(value[i].data, "size=", 5) == 0) {
            s.len = value[i].len - 5;
            s.data = value[i].data + 5;

            size = ngx_parse_size(&s);
            if (size > 8191) {
                continue;
            }
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid parameter \"%V\"", &value[i]);
        return NGX_CONF_ERROR;
    }


    if (size == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"%V\" must have \"size\" parameter",
                           &cmd->name);
        return NGX_CONF_ERROR;
    }

    ctx = ngx_pcalloc(cf->pool, sizeof(ngx_http_tfs_rc_ctx_t));
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    name.data = (u_char *) NGX_HTTP_TFS_RCS_ZONE_NAME;
    name.len = sizeof(NGX_HTTP_TFS_RCS_ZONE_NAME) - 1;

    shm_zone = ngx_shared_memory_add(cf, &name, size,
                                     &ngx_http_tfs_module);
    if (shm_zone == NULL) {
        return NGX_CONF_ERROR;
    }

    shm_zone->init = ngx_http_tfs_rc_server_init_zone;
    shm_zone->data = ctx;

    tmcf->rc_ctx = ctx;

    return NGX_CONF_OK;
}


static char *
ngx_http_tfs_block_cache_zone(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    size_t                                  size;
    ngx_str_t                              *value, s, name;
    ngx_uint_t                              i;
    ngx_shm_zone_t                         *shm_zone;
    ngx_http_tfs_main_conf_t               *tmcf = conf;
    ngx_http_tfs_local_block_cache_ctx_t   *ctx;

    value = cf->args->elts;
    size = 0;

    for (i = 1; i < cf->args->nelts; i++) {

        if (ngx_strncmp(value[i].data, "size=", 5) == 0) {
            s.len = value[i].len - 5;
            s.data = value[i].data + 5;

            size = ngx_parse_size(&s);
            if (size > 8191) {
                continue;
            }
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid parameter \"%V\"", &value[i]);
        return NGX_CONF_ERROR;
    }


    if (size == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"%V\" must have \"size\" parameter",
                           &cmd->name);
        return NGX_CONF_ERROR;
    }

    ctx = ngx_pcalloc(cf->pool, sizeof(ngx_http_tfs_local_block_cache_ctx_t));
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    name.data = (u_char *) NGX_HTTP_TFS_BLOCK_CACHE_ZONE_NAME;
    name.len = sizeof(NGX_HTTP_TFS_BLOCK_CACHE_ZONE_NAME) - 1;

    shm_zone = ngx_shared_memory_add(cf, &name, size,
                                     &ngx_http_tfs_module);
    if (shm_zone == NULL) {
        return NGX_CONF_ERROR;
    }

    shm_zone->init = ngx_http_tfs_local_block_cache_init_zone;
    shm_zone->data = ctx;

    tmcf->local_block_cache_ctx = ctx;

    return NGX_CONF_OK;
}


static void
ngx_http_tfs_read_body_handler(ngx_http_request_t *r)
{
    ngx_int_t             rc;
    ngx_http_tfs_t       *t;
    ngx_connection_t     *c;

    c = r->connection;
    t = ngx_http_get_module_ctx(r, ngx_http_tfs_module);

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http init tfs, client timer: %d", c->read->timer_set);

    if (c->read->timer_set) {
        ngx_del_timer(c->read);
    }

    if (ngx_event_flags & NGX_USE_CLEAR_EVENT) {

        if (!c->write->active) {
            if (ngx_add_event(c->write, NGX_WRITE_EVENT, NGX_CLEAR_EVENT)
                == NGX_ERROR)
            {
                ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                return;
            }
        }
    }

    if (t->r_ctx.large_file || t->r_ctx.fsname.file_type == NGX_HTTP_TFS_LARGE_FILE_TYPE) {
        t->is_large_file = NGX_HTTP_TFS_YES;
    }

    if (r->request_body) {
        t->send_body = r->request_body->bufs;
        if (t->send_body == NULL) {
            ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
            return;
        }
        if (r->headers_in.content_length_n > NGX_HTTP_TFS_USE_LARGE_FILE_SIZE
            && t->r_ctx.version == 1)
        {
            t->is_large_file = NGX_HTTP_TFS_YES;
        }
        /* save large file data len here */
        if (t->is_large_file) {
            t->r_ctx.size = r->headers_in.content_length_n;
        }
    }

    rc = ngx_http_tfs_init(t);

    if (rc != NGX_OK) {
        switch (rc) {
        case NGX_HTTP_SPECIAL_RESPONSE ... NGX_HTTP_INTERNAL_SERVER_ERROR:
            ngx_http_finalize_request(r, rc);
            break;
        default:
            ngx_log_error(NGX_LOG_ERR, t->log, 0,
                          "ngx_http_tfs_init failed");
            ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        }
    }
}


static ngx_int_t
ngx_http_tfs_module_init(ngx_cycle_t *cycle)
{
    ngx_http_tfs_main_conf_t             *tmcf;

    tmcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_tfs_module);
    if (tmcf == NULL) {
        return NGX_ERROR;
    }

    if (tmcf->enable_rcs == NGX_HTTP_TFS_NO) {
        return NGX_OK;
    }

    return ngx_http_tfs_timers_init(cycle, tmcf->lock_file.data);
}


static ngx_int_t
ngx_http_tfs_check_init_worker(ngx_cycle_t *cycle)
{
    ngx_http_tfs_main_conf_t       *tmcf;
    /* rc keepalive */
    tmcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_tfs_module);
    if (tmcf == NULL) {
        return NGX_ERROR;
    }

    if (tmcf->enable_rcs == NGX_HTTP_TFS_NO) {
        return NGX_OK;
    }

    if (tmcf->rcs_kp_enable) {
        return ngx_http_tfs_add_rcs_timers(cycle, tmcf);
    }

    return NGX_OK;
}


#ifdef NGX_HTTP_TFS_USE_TAIR
static void
ngx_http_tfs_check_exit_worker(ngx_cycle_t *cycle)
{
    ngx_int_t                           i;
    ngx_http_tfs_main_conf_t           *tmcf;
    ngx_http_tfs_tair_instance_t       *dup_instance;

    tmcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_tfs_module);
    if (tmcf == NULL) {
        return;
    }

    /* destroy duplicate server */
    for (i = 0; i < NGX_HTTP_TFS_MAX_CLUSTER_COUNT; i++) {
        dup_instance = &tmcf->dup_instances[i];
        if (dup_instance->server != NULL) {
            ngx_http_etair_destory_server(dup_instance->server, (ngx_cycle_t *) ngx_cycle);
        }
    }

    /* destroy remote block cache server */
    if (tmcf->remote_block_cache_instance.server != NULL) {
        ngx_http_etair_destory_server(tmcf->remote_block_cache_instance.server, (ngx_cycle_t *) ngx_cycle);
    }

}
#endif
