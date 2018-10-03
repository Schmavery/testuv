module StringMap = Map.Make(String);

type reqT = {
  url: string,
  headers: StringMap.t(string),
};

type resT = {
  client: UvBind.clientT,
  mutable statusCode: int,
  mutable contentType: string,
  mutable headers: StringMap.t(string),
  mutable wroteHeaders: bool,
  mutable onData: bytes => unit,
  mutable onEnd: unit => unit,
};

type responseListenT('a) =
  | Data: responseListenT(string => unit)
  | End: responseListenT(unit => unit);

let secondArg = (cb, _, v, _) => {
  cb(v);
  0;
};

let createParser = (req_cb, res) => {
  let url = ref("");
  let fields = ref([]);
  let values = ref([]);
  let settings: HttpParser.http_parser_settings = {
    on_message_begin: _ => 0,
    on_url: secondArg(u => url := u),
    on_status: _ => 0,
    on_header_field: secondArg(f => fields := [f, ...fields^]),
    on_header_value: secondArg(v => values := [v, ...values^]),
    on_headers_complete: _ => {
      let req = {
        url: url^,
        headers:
          List.fold_left2(
            (map, f, v) => StringMap.add(f, v, map),
            StringMap.empty,
            fields^,
            values^,
          ),
      };
      req_cb(req, res);
      0;
    },
    on_body: secondArg(body => res.onData(body)),
    on_message_complete: _ => {
      res.onEnd();
      0;
    },
  };
  HttpParser.init(settings, HttpParser.HTTP_REQUEST);
};

let getStatusCodeName = n =>
  switch (n) {
  | 200 => "OK"
  | _ => "Unknown"
  };

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
  UvBind.uv_write(res.client, chunkedMsg);
};

let setHeader = (req, s, v) =>
  req.headers = StringMap.add(s, v, req.headers);

let on = (type a, req, t: responseListenT(a), cb: a) =>
  switch (t) {
  | Data => req.onData = cb
  | End => req.onEnd = cb
  };

let endConnection = r => {
  write(r, "");
  UvBind.uv_shutdown(r.client);
};

let run = (cb: unit => unit) => {
  cb();
  let loop = UvBind.uv_default_loop();
  UvBind.uv_run(loop, UV_RUN_DEFAULT);
};

external request : (string, int, string, (int, bytes) => unit) => unit =
  "request";

exception HttpError(string);
Callback.register_exception("http-exception-type", HttpError("any string"));

let createServer = (cb: (reqT, resT) => unit) => {
  let loop = UvBind.uv_default_loop();
  let server = UvBind.uv_tcp_init(loop);
  let callback = () => {
    let client = UvBind.uv_tcp_init(loop);
    if (UvBind.uv_accept(server, client) == 0) {
      let res = {
        client,
        statusCode: 200,
        contentType: "text/plain",
        headers:
          StringMap.add(
            "Connection",
            "keep-alive",
            StringMap.add("Transfer-Encoding", "chunked", StringMap.empty),
          ),
        wroteHeaders: false,
        onData: _ => (),
        onEnd: () => (),
      };
      let parser = createParser(cb, res);

      UvBind.requestOn(
        client,
        data => {
          let _ = HttpParser.execute(parser, data);
          ();
        },
      );
      UvBind.uv_read_start(client);
    } else {
      UvBind.uv_shutdown(client);
    };
  };
  (callback, server);
};

let listen = ((callback, server), port, host) => {
  let host =
    switch (Unix.gethostbyname(host)) {
    | exception Not_found => host
    | {h_addr_list: [||]} => host
    | {h_addr_list} => Unix.string_of_inet_addr(h_addr_list[0])
    };
  UvBind.uv_tcp_bind(server, port, host, UvBind.af_inet);
  UvBind.uv_listen(server, UvBind.somaxconn, callback);
  let loop = UvBind.uv_default_loop();
  UvBind.uv_run(loop, UV_RUN_DEFAULT);
};
