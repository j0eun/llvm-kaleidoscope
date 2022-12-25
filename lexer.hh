#pragma once

#include <string>

enum Token {
  tok_eof = -1,

  // commands
  tok_def = -2,
  tok_extern = -3,

  // primary
  tok_identifier = -4,
  tok_number = -5
};

int gettok();
int chktok(std::string s);
std::string tok2str(int tok);

extern double NumVal;
extern std::string IdentifierStr;

