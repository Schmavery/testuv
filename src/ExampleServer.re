let server =
  Http.createServer(res => {
    res.contentType = "text/html";
    Http.write(res, "Hello World from libuv in reason");
    Http.on(res, Data, msg => Http.write(res, Bytes.to_string(msg)));
    Http.on(res, End, () => Http.endConnection(res));
  });

Http.listen(server, 8080, "localhost");
