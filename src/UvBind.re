type loopT;
type tcpT;
type clientT = tcpT;
type serverT = tcpT;
type streamT = tcpT;

type uv_run_mode =
  | UV_RUN_DEFAULT
  | UV_RUN_ONCE
  | UV_RUN_NOWAIT;
let somaxconn = 128;
let af_inet = 2;

external write : (clientT, string) => unit = "ocamluv_write";
external endConnection : clientT => unit = "end_connection";
external requestOn : (clientT, bytes => unit) => unit = "request_on";

external uv_default_loop : unit => loopT = "uv_default_loop_ocaml";
external uv_run : (loopT, uv_run_mode) => unit = "uv_run_ocaml";
external uv_tcp_init : loopT => tcpT = "uv_tcp_init_ocaml";
external uv_tcp_bind : (tcpT, int, string, int) => unit = "uv_tcp_bind_ocaml";
external uv_listen : (tcpT, int, unit => unit) => unit = "uv_listen_ocaml";
external uv_accept : streamT => streamT => int = "uv_accept_ocaml";
external uv_shutdown : streamT => unit = "uv_shutdown_ocaml";
external uv_read_start : streamT => unit = "uv_read_start_ocaml";

