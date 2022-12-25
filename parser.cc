#include "parser.hh"
#include "json.hh"
#include "json_fwd.hh"

//===----------------------------------------------------------------------===//
// Parser
//===----------------------------------------------------------------------===//

std::map<char, int> BinopPrecedence;
int CurTok = 0;

/// CurTok/getNextToken - Provide a simple token buffer.  CurTok is the current
/// token the parser is looking at.  getNextToken reads another token from the
/// lexer and updates CurTok with its results.
int getNextToken() { 
    return CurTok = gettok(); 
}

/// GetTokPrecedence - Get the precedence of the pending binary operator token.
int GetTokPrecedence() {
  if (!isascii(CurTok))
    return -1;

  // Make sure it's a declared binop.
  int TokPrec = BinopPrecedence[CurTok];
  if (TokPrec <= 0)
    return -1;
  return TokPrec;
}

/// LogError* - These are little helper functions for error handling.
std::unique_ptr<ExprAST> LogError(const char *Str) {
  fprintf(stderr, "Error: %s\n", Str);
  return nullptr;
}
std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
  LogError(Str);
  return nullptr;
}

std::unique_ptr<ExprAST> ParseExpression();

/// numberexpr ::= number
std::unique_ptr<ExprAST> ParseNumberExpr() {
  auto Result = std::make_unique<NumberExprAST>(NumVal);
  getNextToken(); // consume the number
  return std::move(Result);
}

/// parenexpr ::= '(' expression ')'
std::unique_ptr<ExprAST> ParseParenExpr() {
  getNextToken(); // eat (.
  auto V = ParseExpression();
  if (!V)
    return nullptr;

  if (CurTok != ')')
    return LogError("expected ')'");
  getNextToken(); // eat ).
  return V;
}

/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'
std::unique_ptr<ExprAST> ParseIdentifierExpr() {
  std::string IdName = IdentifierStr;

  getNextToken(); // eat identifier.

  if (CurTok != '(') // Simple variable ref.
    return std::make_unique<VariableExprAST>(IdName);

  // Call.
  getNextToken(); // eat (
  std::vector<std::unique_ptr<ExprAST>> Args;
  if (CurTok != ')') {
    while (true) {
      if (auto Arg = ParseExpression())
        Args.push_back(std::move(Arg));
      else
        return nullptr;

      if (CurTok == ')')
        break;

      if (CurTok != ',')
        return LogError("Expected ')' or ',' in argument list");
      getNextToken();
    }
  }

  // Eat the ')'.
  getNextToken();

  return std::make_unique<CallExprAST>(IdName, std::move(Args));
}

/// primary
///   ::= identifierexpr
///   ::= numberexpr
///   ::= parenexpr
std::unique_ptr<ExprAST> ParsePrimary() {
  switch (CurTok) {
  default:
    return LogError("unknown token when expecting an expression");
  case tok_identifier:
    return ParseIdentifierExpr();
  case tok_number:
    return ParseNumberExpr();
  case '(':
    return ParseParenExpr();
  }
}

/// binoprhs
///   ::= ('+' primary)*
std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
                                              std::unique_ptr<ExprAST> LHS) {
  // If this is a binop, find its precedence.
  while (true) {
    int TokPrec = GetTokPrecedence();

    // If this is a binop that binds at least as tightly as the current binop,
    // consume it, otherwise we are done.
    if (TokPrec < ExprPrec)
      return LHS;

    // Okay, we know this is a binop.
    int BinOp = CurTok;
    getNextToken(); // eat binop

    // Parse the primary expression after the binary operator.
    auto RHS = ParsePrimary();
    if (!RHS)
      return nullptr;

    // If BinOp binds less tightly with RHS than the operator after RHS, let
    // the pending operator take RHS as its LHS.
    int NextPrec = GetTokPrecedence();
    if (TokPrec < NextPrec) {
      RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
      if (!RHS)
        return nullptr;
    }

    // Merge LHS/RHS.
    LHS =
        std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
  }
}

/// expression
///   ::= primary binoprhs
///
std::unique_ptr<ExprAST> ParseExpression() {
  auto LHS = ParsePrimary();
  if (!LHS)
    return nullptr;

  return ParseBinOpRHS(0, std::move(LHS));
}

/// prototype
///   ::= id '(' id* ')'
std::unique_ptr<PrototypeAST> ParsePrototype() {
  if (CurTok != tok_identifier)
    return LogErrorP("Expected function name in prototype");

  std::string FnName = IdentifierStr;
  getNextToken();

  if (CurTok != '(')
    return LogErrorP("Expected '(' in prototype");

  std::vector<std::string> ArgNames;
  while (getNextToken() == tok_identifier)
    ArgNames.push_back(IdentifierStr);
  if (CurTok != ')')
    return LogErrorP("Expected ')' in prototype");

  // success.
  getNextToken(); // eat ')'.

  return std::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}

/// definition ::= 'def' prototype expression
std::unique_ptr<FunctionAST> ParseDefinition() {
  getNextToken(); // eat def.
  auto Proto = ParsePrototype();
  if (!Proto)
    return nullptr;

  if (auto E = ParseExpression())
    return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  return nullptr;
}

/// toplevelexpr ::= expression
std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
  if (auto E = ParseExpression()) {
    // Make an anonymous proto.
    auto Proto = std::make_unique<PrototypeAST>("__anon_expr",
                                                std::vector<std::string>());
    return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  }
  return nullptr;
}

/// external ::= 'extern' prototype
std::unique_ptr<PrototypeAST> ParseExtern() {
  getNextToken(); // eat extern.
  return ParsePrototype();
}

//===----------------------------------------------------------------------===//
// Top-Level parsing
//===----------------------------------------------------------------------===//

void HandleDefinition() {
  auto def = ParseDefinition();
  if (def) {
    nlohmann::json ast;
    // Prototype parsing
    std::string func_name = def->Proto->getName();
    std::vector<std::string> args = def->Proto->getArgs();
    ast["type"] = "tok_def";
    ast["prototype"]["name"]["type"] = tok2str(chktok(func_name)).c_str();
    ast["prototype"]["name"]["value"] = func_name.c_str();
    ast["prototype"]["args"] = nlohmann::json::array();
    for (int i = 0; i < args.size(); i++) {
      nlohmann::json arg;
      arg["type"] = tok2str(chktok(args[i])).c_str();
      arg["value"] = args[i].c_str();
      ast["prototype"]["args"].push_back(arg);
    }
    // Expression parsing
    if (!strcmp(typeid(NumberExprAST).name(), typeid(*def->Body).name())) {
        // std::unique_ptr<ExprAST> clone = std::make_unique<ExprAST>(*def->Body);
        // NumberExprAST* num_expr = dynamic_cast<NumberExprAST*>(clone.get());
        // if (!num_expr) {
        //   fprintf(stderr, "failed under casting to NumberExprAST from ExprAST\n");
        //   return;
        // }
        // ast["expression"]["type"] = tok2str(chktok(std::to_string(num_expr->getNumber()))).c_str();
        // ast["expression"]["value"] = num_expr->getNumber();
    }
    else if (!strcmp(typeid(VariableExprAST).name(), typeid(*def->Body).name())) {
        // VariableExprAST* temp = &(VariableExprAST*)def->Body;
        // ast["expression"]["type"] = tok2str(chktok(temp->getName())).c_str();
        // ast["expression"]["value"] = temp->getName();
    }
    else if (!strcmp(typeid(BinaryExprAST).name(), typeid(*def->Body).name())) {
      // std::unique_ptr<ExprAST> clone = std::make_unique<ExprAST>(*def->Body);
      // BinaryExprAST* bin_expr = dynamic_cast<BinaryExprAST*>(clone.get());
      // if (!bin_expr) {
      //   fprintf(stderr, "failed under casting to BinaryExprAST from ExprAST\n");
      //   return;
      // }
      // ast["expression"]["type"] = tok2str(chktok(std::to_string(bin_expr->getOp()))).c_str();
      // ast["expression"]["value"] = bin_expr->getOp();
    }
    else if (!strcmp(typeid(CallExprAST).name(), typeid(*def->Body).name())) {
    }
    else {
    }
    fprintf(stderr, "%s\n", ast.dump(4).c_str());
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

void HandleExtern() {
  auto ext = ParseExtern();
  if (ext) {
    nlohmann::json ast;
    std::string func_name = ext->getName();
    std::vector<std::string> args = ext->getArgs();
    ast["type"] = "tok_extern";
    ast["prototype"]["name"]["type"] = tok2str(chktok(func_name)).c_str();
    ast["prototype"]["name"]["value"] = func_name.c_str();
    ast["prototype"]["args"] = nlohmann::json::array();
    for (int i = 0; i < args.size(); i++) {
      nlohmann::json arg;
      arg["type"] = tok2str(chktok(args[i])).c_str();
      arg["value"] = args[i].c_str();
      ast["prototype"]["args"].push_back(arg);
    }
    fprintf(stderr, "%s\n", ast.dump(4).c_str());
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

void HandleTopLevelExpression() {
  auto expr = ParseTopLevelExpr();
  // Evaluate a top-level expression into an anonymous function.
  if (expr) {
    // fprintf(stderr, "%llx\n", *expr);
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

/// top ::= definition | external | expression | ';'
void MainLoop() {
  switch (CurTok) {
  case tok_eof:
    return;
  case ';': // ignore top-level semicolons.
    getNextToken();
    break;
  case tok_def:
    HandleDefinition();
    break;
  case tok_extern:
    HandleExtern();
    break;
  default:
    HandleTopLevelExpression();
    break;
  }
  while (true) {
    fprintf(stderr, "ready> ");
    switch (CurTok) {
    case tok_eof:
      return;
    case ';': // ignore top-level semicolons.
      getNextToken();
      break;
    case tok_def:
      HandleDefinition();
      break;
    case tok_extern:
      HandleExtern();
      break;
    default:
      HandleTopLevelExpression();
      break;
    }
  }
}