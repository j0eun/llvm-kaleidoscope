#include "lexer.hh"

//===----------------------------------------------------------------------===//
// Lexer
//===----------------------------------------------------------------------===//

double NumVal = 0.0;
std::string IdentifierStr;

/// gettok - Return the next token from standard input.
int gettok() {
  static int LastChar = ' ';

  // Skip any whitespace.
  while (isspace(LastChar)) {
    LastChar = getchar();
    // fprintf(stdout, "%c", LastChar);
  }

  if (isalpha(LastChar)) { // identifier: [a-zA-Z][a-zA-Z0-9]*
    IdentifierStr = LastChar;
    while (isalnum((LastChar = getchar()))) {
      IdentifierStr += LastChar;
      // fprintf(stdout, "%c", LastChar);
    }

    if (IdentifierStr == "def")
      return tok_def;
    if (IdentifierStr == "extern")
      return tok_extern;
    return tok_identifier;
  }

  if (isdigit(LastChar) || LastChar == '.') { // Number: [0-9.]+
    std::string NumStr;
    do {
      NumStr += LastChar;
      LastChar = getchar();
      // fprintf(stdout, "%c", LastChar);
    } while (isdigit(LastChar) || LastChar == '.');

    NumVal = strtod(NumStr.c_str(), nullptr);
    return tok_number;
  }

  if (LastChar == '#') {
    // Comment until end of line.
    do
      LastChar = getchar();
    while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

    if (LastChar != EOF)
      return gettok();
  }

  // Check for end of file.  Don't eat the EOF.
  if (LastChar == EOF)
    return tok_eof;

  // Otherwise, just return the character as its ascii value.
  int ThisChar = LastChar;
  LastChar = getchar();
  return ThisChar;
}

int chktok(std::string s) {
  if (s.length() == 0) {
    return 0;
  }
  if (isalpha(s[0])) { // identifier: [a-zA-Z][a-zA-Z0-9]*
    std::string _identifierStr;
    _identifierStr = s[0];
    for (int i = 1; i < s.length(); i++) {
      if (isalnum(s[i])) {
        _identifierStr += s[i];
      }
    }
    if (_identifierStr == "def")
      return tok_def;
    if (_identifierStr == "extern")
      return tok_extern;
    return tok_identifier;
  }

  if (isdigit(s[0]) || s[0] == '.') { // Number: [0-9.]+
    return tok_number;
  }

  // Check for end of file.  Don't eat the EOF.
  if (s[0] == EOF)
    return tok_eof;

  return 0;
}

std::string tok2str(int tok) {
  std::string s;

  switch (tok) {
  case tok_eof:
    s = "tok_eof";
    break;
  case tok_def:
    s = "tok_def";
    break;
  case tok_extern:
    s = "tok_extern";
    break;
  case tok_identifier:
    s = "tok_identifier";
    break;
  case tok_number:
    s = "tok_number";
    break;
  default:
    s = "none";
    break;
  }

  return s;
}