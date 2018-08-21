type parserKindT =
  | Http_Request
  | Http_Response
  | Http_Both;

type parserT;

external parserInit : parserKindT => parserT = "ocaml_http_parser_init";
