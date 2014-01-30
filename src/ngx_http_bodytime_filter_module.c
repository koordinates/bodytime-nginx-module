
/*
 * Copyright (C) Robert Coup, Koordinates Limited.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct {
    ngx_flag_t           enable;
} ngx_http_bodytime_conf_t;


typedef struct {
    time_t                            body_start_sec;
    ngx_msec_t                        body_start_msec;
} ngx_http_bodytime_ctx_t;


static ngx_int_t ngx_http_bodytime_add_variables(ngx_conf_t *cf);
static ngx_int_t ngx_http_bodytime_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);

static ngx_int_t ngx_http_bodytime_filter_init(ngx_conf_t *cf);
static void *ngx_http_bodytime_create_conf(ngx_conf_t *cf);
static char *ngx_http_bodytime_merge_conf(ngx_conf_t *cf,
    void *parent, void *child);


static ngx_command_t  ngx_http_bodytime_filter_commands[] = {

    { ngx_string("bodytime"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF
                        |NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_bodytime_conf_t, enable),
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_bodytime_filter_module_ctx = {
    ngx_http_bodytime_add_variables,       /* preconfiguration */
    ngx_http_bodytime_filter_init,         /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_bodytime_create_conf,         /* create location configuration */
    ngx_http_bodytime_merge_conf           /* merge location configuration */
};


ngx_module_t  ngx_http_bodytime_filter_module = {
    NGX_MODULE_V1,
    &ngx_http_bodytime_filter_module_ctx,  /* module context */
    ngx_http_bodytime_filter_commands,     /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_str_t  ngx_http_bodytime = ngx_string("body_start");

static ngx_http_output_body_filter_pt    ngx_http_next_body_filter;


static ngx_int_t
ngx_http_bodytime_body_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngx_http_bodytime_conf_t *conf;
    ngx_http_bodytime_ctx_t  *ctx;
    ngx_time_t               *tp;

    conf = ngx_http_get_module_loc_conf(r, ngx_http_bodytime_filter_module);

    if (conf->enable) {

        ctx = ngx_http_get_module_ctx(r, ngx_http_bodytime_filter_module);

        if (ctx == NULL) {
            // first chain
            // create the context that will follow this request through the chain
            ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_bodytime_ctx_t));
            if (ctx == NULL) {
                return NGX_ERROR;
            }
            ngx_http_set_ctx(r, ctx, ngx_http_bodytime_filter_module);

            // save the current time
            ngx_time_update();      // this call is not "free"
            tp = ngx_timeofday();
            ctx->body_start_sec = tp->sec;
            ctx->body_start_msec = tp->msec;

            ngx_log_debug4(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "http bodytime filter r=%d.%d b=%d.%d",
                           r->start_sec, r->start_msec,
                           ctx->body_start_sec, ctx->body_start_msec);
        }
    }

    return ngx_http_next_body_filter(r, in);
}

static ngx_int_t
ngx_http_bodytime_add_variables(ngx_conf_t *cf)
{
    ngx_http_variable_t  *var;

    var = ngx_http_add_variable(cf, &ngx_http_bodytime, NGX_HTTP_VAR_NOHASH);
    if (var == NULL) {
        return NGX_ERROR;
    }

    var->get_handler = ngx_http_bodytime_variable;

    return NGX_OK;
}


static ngx_int_t
ngx_http_bodytime_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_msec_int_t   ms;

    ngx_http_bodytime_ctx_t  *ctx;

    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    ctx = ngx_http_get_module_ctx(r, ngx_http_bodytime_filter_module);

    if (ctx == NULL) {
        v->not_found = 1;
        return NGX_OK;
    }

    v->data = ngx_pnalloc(r->pool, NGX_INT32_LEN + 3);
    if (v->data == NULL) {
        return NGX_ERROR;
    }

    ms = (ngx_msec_int_t)
             ((ctx->body_start_sec - r->start_sec) * 1000 + (ctx->body_start_msec - r->start_msec));
    ms = ngx_max(ms, 0);

    v->len = ngx_sprintf(v->data, "%T.%03M", (time_t) ms / 1000, ms % 1000) - v->data;

    return NGX_OK;
}


static void *
ngx_http_bodytime_create_conf(ngx_conf_t *cf)
{
    ngx_http_bodytime_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_bodytime_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    conf->enable = NGX_CONF_UNSET;

    return conf;
}


static char *
ngx_http_bodytime_merge_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_bodytime_conf_t *prev = parent;
    ngx_http_bodytime_conf_t *conf = child;

    ngx_conf_merge_value(conf->enable, prev->enable, 0);

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_bodytime_filter_init(ngx_conf_t *cf)
{
    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter = ngx_http_bodytime_body_filter;

    return NGX_OK;
}
