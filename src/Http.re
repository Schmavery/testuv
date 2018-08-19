type serverT;

type resT;

type headerT =
  | HTTP(int, string)
  | ContentType(string)
  | Connection(string)
  | ContentLength(int)
  | Debug(string);

type responseListenT('a) =
  | Data: responseListenT(string => unit)
  | End: responseListenT(unit => unit);

external createServer : (resT => unit) => serverT = "create_server";

external listen_ : (serverT, int, string) => unit = "ocamluv_listen";

let listen = (server, port, host) =>
  switch (Unix.gethostbyname(host)) {
  | exception Not_found => listen_(server, port, host)
  | {h_addr_list: [||]} => listen_(server, port, host)
  | {h_addr_list} =>
    listen_(server, port, Unix.string_of_inet_addr(h_addr_list[0]))
  };

external write : (resT, string) => unit = "ocamluv_write";

let writeHead = (r: resT, l: list(headerT)) => {
  let headers =
    String.concat(
      "\n",
      List.map(
        h =>
          switch (h) {
          | HTTP(i, s) => Printf.sprintf("HTTP/1.0 %d %s", i, s)
          | ContentType(s) => Printf.sprintf("Content-type: %s", s)
          | ContentLength(i) => Printf.sprintf("Content-length: %d", i)
          | Connection(s) => Printf.sprintf("Connection: %s", s)
          | Debug(s) => s
          },
        l,
      ),
    )
    ++ "\n\n";
  print_endline(headers);
  write(r, headers);
};

/* TODO: Make res different than req? */
external requestOnData : (resT, bytes => unit) => unit = "on_data";

external requestOnEnd : (resT, unit => unit) => unit = "on_end";

let requestOn = (type a, req, t: responseListenT(a), cb: a) =>
  switch (t) {
  | Data => requestOnData(req, cb)
  | End => requestOnEnd(req, cb)
  };

external endConnection : resT => unit = "end_connection";
