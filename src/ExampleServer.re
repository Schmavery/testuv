let printHeaders = h =>
  Http.StringMap.fold(
    Printf.sprintf("%s: %s<br>%s"),
    h,
    "",
  );

let server =
  Http.createServer((req, res) => {
    res.contentType = "text/html";
    Http.write(res, "Hello World from libuv in reason");
    Http.write(res, "<br>Url: " ++ req.url);
    Http.write(res, "<br>Headers:<br>" ++ printHeaders(req.headers));
    Http.on(res, Data, msg => Http.write(res, "testdata"));
    Http.on(res, End, () => Http.endConnection(res));
    /* Http.endConnection(res); */
  });

Http.listen(server, 8080, "localhost");
