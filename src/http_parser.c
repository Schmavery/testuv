#include <unistd.h>
#include <stdio.h>

// A bunch of imports I probably won't use
#include <caml/alloc.h>
#include <caml/bigarray.h>
#include <caml/custom.h>
#include <caml/memory.h>
#include <caml/mlvalues.h>
#include <caml/callback.h>

#include "../http-parser/http_parser.h"

#define LOG(m) fprintf(stderr, "%s\n", m)

CAMLprim void ocaml_http_parser_init(value type){
  CAMLparam1(type);
  CAMLlocal1(parserWrapper);

  // Do the actual init call:
  // The ocaml enum is the same order as the C one so the
  // int representation is the same.
  http_parser *parser = malloc(sizeof(http_parser));
  http_parser_init(parser, Int_val(type))

  // Wrap the parser pointer in an ocaml object
  // (Abstract so that the gc doesn't try to collect it)
  parserWrapper = caml_alloc(1, Abstract_tag);
  Field(parserWrapper, 0) = (long) parser;

  // Pass back the parser
  CAMLreturn(parser);
}
