// Some rough equivalent in node for benchmarking or something
var http = require('http');

//create a server object:
http.createServer(function (req, res) {
  res.setHeader('Content-Type', 'text/html');
  console.log("writing output");
  res.write('Hello World!'); //write a response to the client
  req.on('data', (d) => res.write(d));
  req.on('end', () => res.end());
  // res.end();
}).listen(8080); //the server object listens on port 8080
