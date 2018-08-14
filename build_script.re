open Bsb_internals;


let ( +/ ) = Filename.concat;

gcc("lib" +/ "libuv_c.o", ["src" +/ "libuv_c.c"]);
