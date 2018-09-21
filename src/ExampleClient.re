Http.run(() =>
  Http.request(
    "127.0.0.1",
    8080,
    "GET / HTTP/1.1\r\nHost: localhost:8080\r\n\r\n",
    (error, data) =>
    if (error != 0) {
      print_endline("Error: " ++ string_of_int(error));
    } else {
      print_endline("Success: " ++ data);
    }
  )
);
