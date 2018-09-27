/* TODO replace with async readfile from libuv */
let rec readFile = (chan, acc) =>
  switch (input_line(chan)) {
  | s => readFile(chan, [s, ...acc])
  | exception End_of_file =>
    close_in(chan);
    String.concat("\n", List.rev(acc));
  };

let readFile = filename => readFile(open_in(filename), []);

let server =
  Http.createServer((req, res) => {
    res.contentType = "text/html";
    let url =
      req.url
      ++ (req.url.[String.length(req.url) - 1] == '/' ? "index.html" : "");
    print_endline("Serving " ++ url);
    switch (readFile("." ++ url)) {
    | content => Http.write(res, content)
    | exception (Sys_error(_)) =>
      res.statusCode = 404;
      Http.write(res, "Couldn't find that!");
    };
    Http.endConnection(res);
  });

Http.listen(server, 8080, "localhost");
