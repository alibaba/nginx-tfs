<<<<<<< HEAD:ngx_http_tfs_meta_server_message.h

/*
 * Copyright (C) 2010-2013 Alibaba Group Holding Limited
 */


#ifndef _NGX_HTTP_TFS_META_SERVER_MESSAGE_H_INCLUDED_
#define _NGX_HTTP_TFS_META_SERVER_MESSAGE_H_INCLUDED_


#include <ngx_http_tfs.h>


ngx_chain_t *ngx_http_tfs_meta_server_create_message(ngx_http_tfs_t *t);
ngx_int_t ngx_http_tfs_meta_server_parse_message(ngx_http_tfs_t *t);
ngx_http_tfs_inet_t *ngx_http_tfs_select_meta_server(ngx_http_tfs_t *t);


#endif  /* _NGX_HTTP_TFS_META_SERVER_MESSAGE_H_INCLUDED_ */
=======

/*
 * Copyright (C) 2010-2012 Alibaba Group Holding Limited
 */


#ifndef _NGX_HTTP_TFS_META_SERVER_MESSAGE_H_INCLUDED_
#define _NGX_HTTP_TFS_META_SERVER_MESSAGE_H_INCLUDED_


#include <ngx_http_tfs.h>


ngx_chain_t *ngx_http_tfs_meta_server_create_message(ngx_http_tfs_t *t);
ngx_int_t ngx_http_tfs_meta_server_parse_message(ngx_http_tfs_t *t);
ngx_http_tfs_inet_t *ngx_http_tfs_select_meta_server(ngx_http_tfs_t *t);


#endif  /* _NGX_HTTP_TFS_META_SERVER_MESSAGE_H_INCLUDED_ */
>>>>>>> 88b88637f9b6aa208a109d18da5113750b4ba69e:src/ngx_http_tfs_meta_server_message.h