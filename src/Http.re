module StringMap = Map.Make(String);

type serverT;

type clientT;

type resT = {
  client: clientT,
  mutable statusCode: int,
  mutable contentType: string,
  mutable headers: StringMap.t(string),
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
      headers:
        StringMap.add(
          "Connection",
          "keep-alive",
          StringMap.add("Transfer-Encoding", "chunked", StringMap.empty),
        ),
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

/* Ocaml why you no strftime */
/* Example: Wed, 22 Aug 2018 08:00:12 GMT */
let dateToStr = (t: Unix.tm) => {
  let weekdays = [|"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"|];
  let months = [|
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec",
  |];
  Printf.sprintf(
    "%s, %d %s %d %02d:%02d:%02d GMT",
    weekdays[t.tm_wday],
    t.tm_mday,
    months[t.tm_mon],
    t.tm_year + 1900, /* wat */
    t.tm_hour,
    t.tm_min,
    t.tm_sec,
  );
};

let write = (res, msg) => {
  let headerStr =
    if (! res.wroteHeaders) {
      res.wroteHeaders = true;
      let http =
        "HTTP/1.1 "
        ++ string_of_int(res.statusCode)
        ++ " "
        ++ getStatusCodeName(res.statusCode)
        ++ "\n";
      let contentType = "Content-type: " ++ res.contentType ++ "\n";
      let date = "Date: " ++ dateToStr(Unix.gmtime(Unix.time())) ++ "\n";
      let headers =
        StringMap.fold(
          (k, v, a) => k ++ ": " ++ v ++ "\n" ++ a,
          res.headers,
          "\n",
        );
      http ++ contentType ++ date ++ headers;
    } else {
      "";
    };
  let chunkedMsg =
    switch (StringMap.find("Transfer-Encoding", res.headers)) {
    | "chunked" =>
      Printf.sprintf("%s%X\r\n%s\r\n", headerStr, String.length(msg), msg)
    | _ => headerStr ++ msg
    | exception Not_found => headerStr ++ msg
    };
  write(res.client, chunkedMsg);
};

let setHeader = (req, s, v) =>
  req.headers = StringMap.add(s, v, req.headers);

/* TODO: Make res different than req? */
external requestOnData : (clientT, bytes => unit) => unit = "on_data";

external requestOnEnd : (clientT, unit => unit) => unit = "on_end";

let requestOn = (type a, req, t: responseListenT(a), cb: a) =>
  switch (t) {
  | Data => requestOnData(req.client, cb)
  | End => requestOnEnd(req.client, cb)
  };

external endConnection : clientT => unit = "end_connection";

let endConnection = r => {
  write(r, "");
  endConnection(r.client)
};

external request : string => int => string => unit = "request";
