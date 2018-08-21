type serverT;

type clientT;

type resT = {
  client: clientT,
  mutable statusCode: int,
  mutable contentType: string,
  mutable headers: string,
  mutable wroteHeaders: bool,
};

type responseListenT('a) =
  | Data: responseListenT(string => unit)
  | End: responseListenT(unit => unit);

external createServer : (clientT => unit) => serverT = "create_server";

let createServer = cb =>
  createServer(c =>
    cb({
      client: c,
      statusCode: 200,
      contentType: "text/plain",
      headers: "\n",
      wroteHeaders: false,
    })
  );

external listen : (serverT, int, string) => unit = "ocamluv_listen";

let listen = (server, port, host) =>
  switch (Unix.gethostbyname(host)) {
  | exception Not_found => listen(server, port, host)
  | {h_addr_list: [||]} => listen(server, port, host)
  | {h_addr_list} =>
    listen(server, port, Unix.string_of_inet_addr(h_addr_list[0]))
  };

let getStatusCodeName = n =>
  switch (n) {
  | 200 => "OK"
  | _ => "Unknown"
  };

external write : (clientT, string) => unit = "ocamluv_write";

let write = (res, msg) => {
  let fullMsg =
    if (! res.wroteHeaders) {
      res.wroteHeaders = true;
      let http =
        "HTTP/1.0 "
        ++ string_of_int(res.statusCode)
        ++ " "
        ++ getStatusCodeName(res.statusCode)
        ++ "\n";
      let contentType = "Content-type: " ++ res.contentType ++ "\n";
      http ++ contentType ++ res.headers ++ msg;
    } else {
      msg;
    };
  write(res.client, fullMsg);
};

let addHeader = (req, s, v) =>
  req.headers = s ++ ": " ++ v ++ req.headers ++ "\n";

/* TODO: Make res different than req? */
external requestOnData : (clientT, bytes => unit) => unit = "on_data";

external requestOnEnd : (clientT, unit => unit) => unit = "on_end";

let requestOn = (type a, req, t: responseListenT(a), cb: a) =>
  switch (t) {
  | Data => requestOnData(req.client, cb)
  | End => requestOnEnd(req.client, cb)
  };

external endConnection : clientT => unit = "end_connection";

let endConnection = (r) => endConnection(r.client);
