open Bsb_internals;

let (+/) = Filename.concat;

gcc("lib" +/ "libuv_c.o", ~flags=["-g"], ["src" +/ "libuv_c.c"]);

if (! Sys.file_exists("http-parser" +/ "http_parser.c")) {
  prerr_endline("Error: Download git submodules before building.");
  exit(1);
};

/* Taken from http_parser makefile */
/* cc -I. -DHTTP_PARSER_STRICT=0 -Wall -Wextra -Werror -O3 -fPIC -c http_parser.c -o libhttp_parser.o */
gcc("lib" +/ "libhttp_parser.o", ~flags=["-DHTTP_PARSER_STRICT=0", "-O3", "-fPIC"], ["http-parser" +/ "http_parser.c"]);
gcc("lib" +/ "http_parser_stubs.o", ["src" +/ "http_parser_stubs.c"]);
