#include <unistd.h>
#include <stdio.h> // TODO: remove this

#include <caml/alloc.h>
#include <caml/bigarray.h>
#include <caml/custom.h>
#include <caml/memory.h>
#include <caml/mlvalues.h>
#include <caml/callback.h>

#include "../include/uv.h"

/* TODO: unify client and server as much as possible */


typedef struct {
  uv_write_t req;
  uv_buf_t buf;
} write_req_t;

typedef struct {
  uv_connect_t req;
  uv_buf_t buf;
} client_connect_req_t;

typedef struct {
  value data_cb;
  value end_cb;
} client_cbs_t;

#define LOG(m) fprintf(stderr, "%s\n", m)

#define CHECK(r, msg) if (r) {                                                       \
  fprintf(stderr, "%s: [%s(%d): %s]\n", msg, uv_err_name((r)), (int) r, uv_strerror((r))); \
  exit(1);                                                                           \
}

static void close_cb(uv_handle_t* client) {
  client_cbs_t * cbs = (client_cbs_t *)client->data;
  if (cbs->data_cb){
    caml_remove_global_root(&cbs->data_cb);
  }
  if (cbs->end_cb){
    caml_remove_global_root(&cbs->end_cb);
  }
  free(client->data);
  free(client);
  fprintf(stderr, "Closed connection\n");
}

static void shutdown_cb(uv_shutdown_t* req, int status) {
  uv_close((uv_handle_t*) req->handle, close_cb);
  free(req);
}

static void write_cb(uv_write_t *req, int status) {
  CHECK(status, "write_cb");
  /* Since the req is the first field inside the wrapper write_req, we can just cast to it */
  fprintf(stderr, "Actually wrote things\n");
  write_req_t *write_req = (write_req_t*) req;
  free(write_req->buf.base);
  free(write_req);
}

static void alloc_cb(uv_handle_t *handle, size_t size, uv_buf_t *buf) {
  // TODO: doublecheck this works: Allocate 1 extra byte to have a null-terminated sequence.
  buf->base = calloc(size+1, 1);
  if (buf->base == NULL) fprintf(stderr, "alloc_cb buffer didn't properly initialize");
  buf->len = size;
}

static void read_cb(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf) {
  CAMLparam0();
  CAMLlocal1(read_str);
  int r = 0;
  LOG("Read_cb running\n");

  /* Errors or EOF */
  if (nread < 0) {
    if (nread != UV_EOF) CHECK(nread, "read_cb");

    /* Client signaled that all data has been sent, so we can close the connection and are done */
    client_cbs_t * cbs = (client_cbs_t *)client->data;
    LOG(">>>> It's all over");

    if (cbs->end_cb) caml_callback(cbs->end_cb, Val_unit);
    if (buf->base) free(buf->base);
    return;
  }

  if (nread == 0) {
    /* Everything OK, but nothing read and thus we don't write anything */
    free(buf->base);
    return;
  }

  /* TODO: maybe this should be a different datatype */
  client_cbs_t * cbs = (client_cbs_t *)client->data;
  if (nread > 0 && cbs->data_cb){
    read_str = caml_copy_string(buf->base);
    caml_callback(cbs->data_cb, read_str);
  }
  free(buf->base);
  CAMLreturn0;
}


static void connection_cb(uv_stream_t *server, int status) {
  CAMLparam0();
  CAMLlocal1(res);
  CHECK(status, "connection_cb");
  uv_shutdown_t *shutdown_req;

  /* Accept client connection */
  fprintf(stderr, "Accepting Connection\n");

  /* Init client connection using `server->loop`, passing the client handle */
  uv_tcp_t *client = malloc(sizeof(uv_tcp_t));
  client_cbs_t *cbs = calloc(1, sizeof(client_cbs_t));
  client->data = cbs;
  int r = uv_tcp_init(server->loop, client);
  CHECK(r, "uv_tcp_init");

  fprintf(stderr, "Initialized client\n");

  /* 4.2. Accept the now initialized client connection */
  r = uv_accept(server, (uv_stream_t*) client);
  if (r) {
    fprintf(stderr, "trying to accept connection %d", r);

    shutdown_req = malloc(sizeof(uv_shutdown_t));
    r = uv_shutdown(shutdown_req, (uv_stream_t*) client, shutdown_cb);
    CHECK(r, "uv_shutdown");
  }


  /* TODO: add a finalize and make it a Custom_tag? */
  /* TODO: put a client_cbs struct on the client->data */
  res = caml_alloc(1, Abstract_tag);
  Field(res, 0) = (long) client;
  caml_callback((value) server->data, res);

  /* Start reading after allowing the user to set up listeners. */
  r = uv_read_start((uv_stream_t*) client, alloc_cb, read_cb);
  CHECK(r, "uv_read_start");

  CAMLreturn0;
}

/* TODO: handle failures using ocaml exceptions */
CAMLprim value create_server(value listen_cb){
  CAMLparam0();
  CAMLlocal1(server);
  /* TODO: add a finalize to free some stuff */
  server = caml_alloc(1, Abstract_tag);

  uv_loop_t *loop = uv_default_loop();
  uv_tcp_t *tcp_server = malloc(sizeof(uv_tcp_t));
  int r = uv_tcp_init(loop, tcp_server);
  CHECK(r, "uv_tcp_init");

  caml_register_global_root(&listen_cb);
  // TODO: make this a struct possibly
  tcp_server->data = (void *)listen_cb;

  Field(server, 0) = (long) tcp_server;
  CAMLreturn(server);
}

CAMLprim void ocamluv_listen(value server, value port, value host){
  CAMLparam3(server, port, host);
  uv_tcp_t *tcp_server = (uv_tcp_t*)Field(server, 0);

  struct sockaddr_in addr;
  fprintf(stderr, "Creating address: %s, %d\n", String_val(host), Int_val(port));
  int r = uv_ip4_addr(String_val(host), Int_val(port), &addr);
  CHECK(r, "uv_ip4_addr");

  // TODO: unhardcode AF_INET?
  r = uv_tcp_bind((uv_tcp_t*) tcp_server, (struct sockaddr*) &addr, AF_INET);
  CHECK(r, "uv_tcp_bind");

  // TODO: unhardcode SOMAXCONN?
  r = uv_listen((uv_stream_t*) tcp_server, SOMAXCONN, connection_cb);
  CHECK(r, "uv_listen");

  r = uv_run(tcp_server->loop, UV_RUN_DEFAULT);
  CHECK(r, "uv_run");

  CAMLreturn0;
}

CAMLprim void ocamluv_write(value res, value str){
  CAMLparam2(res, str);

  size_t buf_len = caml_string_length(str);
  char *buf = calloc(sizeof(char), buf_len);
  memcpy(buf, String_val(str), buf_len);

  write_req_t *write_req = malloc(sizeof(write_req_t));
  write_req->buf = uv_buf_init(buf, buf_len);
  uv_stream_t *client = (uv_stream_t*)Field(res, 0);
  int r = uv_write(&write_req->req, client, &write_req->buf, 1, write_cb);
  CHECK(r, "uv_write");
  CAMLreturn0;
}

CAMLprim void end_connection(value res){
  CAMLparam1(res);
  uv_tcp_t *client = (uv_tcp_t*)Field(res, 0);
  uv_shutdown_t *shutdown_req = malloc(sizeof(uv_shutdown_t));
  int r = uv_shutdown(shutdown_req, (uv_stream_t*) client, shutdown_cb);
  CHECK(r, "uv_shutdown");
  CAMLreturn0;
}

/* TODO: Note.. req and res are currently the same things... */
CAMLprim void on_data(value req, value data_cb){
  CAMLparam2(req, data_cb);
  uv_tcp_t *client = (uv_tcp_t*)Field(req, 0);
  client_cbs_t * cbs = (client_cbs_t *)client->data;
  caml_register_global_root(&data_cb);
  cbs->data_cb = data_cb;
  CAMLreturn0;
}

CAMLprim void on_end(value req, value end_cb){
  CAMLparam2(req, end_cb);
  uv_tcp_t *client = (uv_tcp_t*)Field(req, 0);
  client_cbs_t * cbs = (client_cbs_t *)client->data;
  caml_register_global_root(&end_cb);
  cbs->end_cb = end_cb;
  CAMLreturn0;
}

void timeout_fired_cb(uv_timer_t* timer) {
  LOG("timeout fired");
  /* TODO cleanup, pass error to ocaml listener */
  /* SimpleHttpRequest *client = (SimpleHttpRequest*)timer->data; */
  /* client->_clearTimer(); */
  /* client->_clearConnection(); */
  /* int _err = -ETIMEDOUT; */
  /* client->emit("error", uv_err_name(_err), uv_strerror(_err)); */
}


/* TODO: handle reading chunked encoding? maybe http-parser handles it */
void client_read_cb(uv_stream_t *tcp, ssize_t nread, const uv_buf_t * buf) {
  if (nread < 0) {
    if (nread != UV_EOF) CHECK(nread, "client_read_cb");

    /* TODO: Server signaled that all data has been sent, so we can close the connection and are done */
    /* client_cbs_t * cbs = (client_cbs_t *)client->data; */
    LOG(">>>> It's all over");

    /* if (cbs->end_cb) caml_callback(cbs->end_cb, Val_unit); */
    if (buf->base) free(buf->base);
    return;
  }

  if (nread == 0) {
    /* Everything OK, but nothing read so we don't write anything */
    free(buf->base);
    return;
  }

  /* TODO hook this up to ocaml */
  /* client_cbs_t * cbs = (client_cbs_t *)client->data; */
  /* if (nread > 0 && cbs->data_cb){ */
  /*   read_str = caml_copy_string(buf->base); */
  /*   caml_callback(cbs->data_cb, read_str); */
  /* } */
  fprintf(stderr, "From server: %s", buf->base);
  free(buf->base);
}

void client_connect_cb(uv_connect_t *req, int status) {
  CHECK(status, "client_connect_cb")
  LOG("Connection with server established");
  int r = uv_read_start(req->handle, alloc_cb, client_read_cb);
  CHECK(r, "uv_read_start");

  /* Since the req is the first field inside the wrapper write_req, we can just cast to it */
  client_connect_req_t *connect_req = (client_connect_req_t*) req;

  write_req_t *write_req = malloc(sizeof(write_req_t));
  write_req->buf = connect_req->buf;
  r = uv_write(&write_req->req, req->handle, &write_req->buf, 1, write_cb);
  CHECK(r, "uv_write");

  free(connect_req);
}


int default_timeout = 60*2*1000; // 2 minutes

/* TODO: Split this up into a reason binding for each
 * function and put the logic in reason */
/* TODO: make this not blocking */
CAMLprim void request(value hostv, value portv, value request_str){
  CAMLparam3(hostv, portv, request_str);

  uv_loop_t *loop = uv_default_loop();

  char* host = String_val(hostv);
  int port = Int_val(portv);
  uv_timer_t timer;
  int r = uv_timer_init(loop, &timer);
  CHECK(r, "uv_timer");
  r = uv_timer_start(&timer, timeout_fired_cb, default_timeout, 0);
  CHECK(r, "uv_timer_start");

  struct sockaddr_in dest;
  r = uv_ip4_addr(host, port, &dest);

  if (r != 0) {
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    char* portstr = malloc(10*sizeof(char));
    sprintf(portstr, "%d", port);
    uv_getaddrinfo_t getaddrinfo_req;
    r = uv_getaddrinfo(loop,
                        &getaddrinfo_req,
                        NULL,  // TODO: async?
                        host,
                        portstr,
                        &hints);
    CHECK(r, "uv_getaddrinfo")
    free(portstr);

    char ip[17];
    r = uv_ip4_name((struct sockaddr_in*) getaddrinfo_req.addrinfo->ai_addr, ip, 16);
    CHECK(r, "uv_ip4_name");
    uv_freeaddrinfo(getaddrinfo_req.addrinfo);

    r = uv_ip4_addr(ip, port, &dest);
    CHECK(r, "uv_ip4_addr");
  }

  uv_tcp_t *tcp = malloc(sizeof(uv_tcp_t));
  r = uv_tcp_init(loop, tcp);
  CHECK(r, "uv_tcp_init");

  size_t buf_len = caml_string_length(request_str);
  char *buf = calloc(sizeof(char), buf_len);
  memcpy(buf, String_val(request_str), buf_len);
  client_connect_req_t *connect_req = malloc(sizeof(client_connect_req_t));
  connect_req->buf = uv_buf_init(buf, buf_len);
  r = uv_tcp_connect((uv_connect_t *) connect_req, tcp, (const struct sockaddr *) &dest, client_connect_cb);
  CHECK(r, "uv_tcp_connect");

  r = uv_run(tcp->loop, UV_RUN_DEFAULT);
  CHECK(r, "uv_run request")

  CAMLreturn0;
}
