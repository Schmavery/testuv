/* TODO replace with async readfile from libuv */
let rec readFile = (chan, acc) =>
  switch (input_line(chan)) {
  | s => readFile(chan, [s, ...acc])
  | exception End_of_file =>
    close_in(chan);
    String.concat("\n", List.rev(acc));
  };

let readFile = filename => readFile(open_in(filename), []);

let contentType = path =>
  switch (String.rindex(path, '.')) {
  | exception Not_found => "text/plain"
  | i =>
    switch (String.sub(path, i+1, String.length(path) - i - 1)) {
    | "html" => "text/html"
    | "htm" => "text/html"
    | "css" => "text/css"
    | "mp4" => "video/mp4"
    | "js" => "application/javascript"
    | s =>
      print_endline(s);
      "text/plain";
    }
  };

let server =
  Http.createServer((req, res) => {
    let url =
      req.url
      ++ (req.url.[String.length(req.url) - 1] == '/' ? "index.html" : "");
    print_endline("Serving " ++ url);
    switch (readFile("." ++ url)) {
    | content =>
      res.contentType = contentType(url);
      Http.write(res, content);
    | exception (Sys_error(_)) =>
      res.statusCode = 404;
      Http.write(res, "Couldn't find that!");
    };
    Http.endConnection(res);
  });

Http.listen(server, 8080, "localhost");
