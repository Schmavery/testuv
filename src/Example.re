let server =
  Http.createServer(res => {
    Http.writeHead(
      res,
      [
        HTTP(200, "OK"),
        ContentType("text/html"),
        Connection("keep-alive"),
      ],
    );
    Http.write(res, "Hello World from libuv in reason\n");
    Http.requestOn(res, Data, msg => Http.write(res, Bytes.to_string(msg)));
    Http.requestOn(res, End, () => Http.endConnection(res));
  });

Http.listen(server, 8080, "localhost");
