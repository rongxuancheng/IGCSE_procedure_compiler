#pragma once
#include "Lexer.h"
#include "AST.h"
#include <vector>

class Parser {
public:
    Parser(std::vector<Token> tokens, DiagEngine& diag);

    StmtPtr parseProgram();

private:
    std::vector<Token> tokens_;
    size_t             pos_ = 0;
    DiagEngine&        diag_;

    // ── Token helpers ──────────────────────────────────────────────────
    const Token& peek(int offset = 0) const;
    const Token& advance();
    bool check(TokenType t) const;
    bool checkAny(std::initializer_list<TokenType> types) const;
    bool match(TokenType t);
    Token expect(TokenType t, const std::string& msg);
    void skipNewlines();

    // ── Statement parsers ──────────────────────────────────────────────
    StmtPtr parseBlock(std::initializer_list<TokenType> terminators);
    StmtPtr parseStatement();

    StmtPtr parseDeclare();
    StmtPtr parseConstant();
    StmtPtr parseAssign(const Token& idTok);
    StmtPtr parseOutput();
    StmtPtr parseInput();
    StmtPtr parseIf();
    StmtPtr parseFor();
    StmtPtr parseWhile();
    StmtPtr parseRepeat();
    StmtPtr parseCase();
    StmtPtr parseCall();
    StmtPtr parseReturn();
    StmtPtr parseFunction();
    StmtPtr parseProcedure();

    // ── Expression parsers (Pratt / recursive descent) ─────────────────
    ExprPtr parseExpr();
    ExprPtr parseOr();
    ExprPtr parseAnd();
    ExprPtr parseNot();
    ExprPtr parseComparison();
    ExprPtr parseConcat();   // & operator
    ExprPtr parseAddSub();
    ExprPtr parseMulDiv();
    ExprPtr parsePower();
    ExprPtr parseUnary();
    ExprPtr parsePostfix();  // array index, function call
    ExprPtr parsePrimary();

    // ── Type parsing ───────────────────────────────────────────────────
    TypeSpec parseTypeSpec();

    // ── Param list ─────────────────────────────────────────────────────
    std::vector<Param> parseParamList();

    // Error recovery: skip to next newline
    void syncToNewline();
};
