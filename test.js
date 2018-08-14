// Some rough equivalent in node for benchmarking or something
var http = require('http');

//create a server object:
http.createServer(function (req, res) {
  // res.contentType = "text/html";
  res.setHeader('Content-Type', 'text/html');
  res.write('Hello World!'); //write a response to the client
  req.on('data', (d) => console.log(d));
  res.end();
}).listen(8080); //the server object listens on port 8080
