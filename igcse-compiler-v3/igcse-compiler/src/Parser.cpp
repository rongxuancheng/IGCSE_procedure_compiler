#include "Parser.h"
#include <stdexcept>
#include <cassert>

// ─── Constructor ──────────────────────────────────────────────────────────────
Parser::Parser(std::vector<Token> tokens, DiagEngine& diag)
    : tokens_(std::move(tokens)), diag_(diag) {}

// ─── Token helpers ────────────────────────────────────────────────────────────
const Token& Parser::peek(int offset) const {
    size_t idx = pos_ + offset;
    if (idx >= tokens_.size()) return tokens_.back(); // EOF
    return tokens_[idx];
}
const Token& Parser::advance() {
    const Token& t = tokens_[pos_];
    if (pos_ + 1 < tokens_.size()) pos_++;
    return t;
}
bool Parser::check(TokenType t) const { return peek().type == t; }
bool Parser::checkAny(std::initializer_list<TokenType> types) const {
    for (auto t : types) if (check(t)) return true;
    return false;
}
bool Parser::match(TokenType t) {
    if (check(t)) { advance(); return true; }
    return false;
}
Token Parser::expect(TokenType t, const std::string& msg) {
    if (check(t)) return advance();
    const auto& cur = peek();
    diag_.error(cur.line, cur.col,
        msg + "，但遇到了 " + cur.typeName());
    // return a dummy token to allow parsing to continue
    return Token{t, "", cur.line, cur.col};
}
void Parser::skipNewlines() {
    while (check(TokenType::NEWLINE)) advance();
}
void Parser::syncToNewline() {
    while (!checkAny({TokenType::NEWLINE, TokenType::END_OF_FILE})) advance();
}

// ─── Program ─────────────────────────────────────────────────────────────────
StmtPtr Parser::parseProgram() {
    skipNewlines();
    auto block = std::make_unique<Stmt>();
    block->kind = StmtKind::Block;
    block->line = 1; block->col = 1;

    while (!check(TokenType::END_OF_FILE)) {
        try {
            auto s = parseStatement();
            if (s) block->body.push_back(std::move(s));
        } catch (...) {
            syncToNewline();
        }
        skipNewlines();
    }
    return block;
}

// ─── Block ────────────────────────────────────────────────────────────────────
StmtPtr Parser::parseBlock(std::initializer_list<TokenType> terminators) {
    auto block = std::make_unique<Stmt>();
    block->kind = StmtKind::Block;
    block->line = peek().line; block->col = peek().col;

    auto isTerminator = [&]() {
        for (auto t : terminators) if (check(t)) return true;
        return check(TokenType::END_OF_FILE);
    };

    skipNewlines();
    while (!isTerminator()) {
        try {
            auto s = parseStatement();
            if (s) block->body.push_back(std::move(s));
        } catch (...) {
            syncToNewline();
        }
        skipNewlines();
    }
    return block;
}

// ─── Statement dispatcher ─────────────────────────────────────────────────────
StmtPtr Parser::parseStatement() {
    skipNewlines();
    const Token& t = peek();

    switch (t.type) {
        case TokenType::KW_DECLARE:   return parseDeclare();
        case TokenType::KW_CONSTANT:  return parseConstant();
        case TokenType::KW_OUTPUT:
        case TokenType::KW_PRINT:     return parseOutput();
        case TokenType::KW_INPUT:     return parseInput();
        case TokenType::KW_IF:        return parseIf();
        case TokenType::KW_FOR:       return parseFor();
        case TokenType::KW_WHILE:     return parseWhile();
        case TokenType::KW_REPEAT:    return parseRepeat();
        case TokenType::KW_CASE:      return parseCase();
        case TokenType::KW_CALL:      return parseCall();
        case TokenType::KW_RETURN:    return parseReturn();
        case TokenType::KW_FUNCTION:  return parseFunction();
        case TokenType::KW_PROCEDURE: return parseProcedure();
        case TokenType::IDENTIFIER: {
            Token idTok = advance();
            // Check for assignment or procedure-call-without-CALL
            if (checkAny({TokenType::ASSIGN})) {
                return parseAssign(idTok);
            } else if (check(TokenType::LBRACKET)) {
                // Array assignment: arr[i] ← val
                return parseAssign(idTok);
            } else if (check(TokenType::LPAREN)) {
                // implicit call (without CALL keyword)
                auto s = std::make_unique<Stmt>();
                s->kind = StmtKind::ProcCall;
                s->line = idTok.line; s->col = idTok.col;
                s->callName = idTok.value;
                advance(); // (
                if (!check(TokenType::RPAREN)) {
                    s->callArgs.push_back(parseExpr());
                    while (match(TokenType::COMMA))
                        s->callArgs.push_back(parseExpr());
                }
                expect(TokenType::RPAREN, "期望 ')'");
                return s;
            } else {
                diag_.error(idTok.line, idTok.col,
                    "标识符 '" + idTok.value + "' 后期望 '←' 或 '('");
                syncToNewline();
                return nullptr;
            }
        }
        case TokenType::NEWLINE:
            advance();
            return nullptr;
        default:
            diag_.error(t.line, t.col,
                "意外的符号 " + t.typeName() + "，期望语句");
            syncToNewline();
            return nullptr;
    }
}

// ─── DECLARE ─────────────────────────────────────────────────────────────────
StmtPtr Parser::parseDeclare() {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::Declare;
    s->line = peek().line; s->col = peek().col;
    advance(); // DECLARE

    Token name = expect(TokenType::IDENTIFIER, "期望变量名");
    s->name = name.value;
    expect(TokenType::COLON, "期望 ':' 在变量名后");
    s->typeSpec = parseTypeSpec();
    return s;
}

// ─── CONSTANT ────────────────────────────────────────────────────────────────
StmtPtr Parser::parseConstant() {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::Constant;
    s->line = peek().line; s->col = peek().col;
    advance(); // CONSTANT

    Token name = expect(TokenType::IDENTIFIER, "期望常量名");
    s->name = name.value;
    expect(TokenType::EQ, "期望 '=' 在常量名后");
    s->initExpr = parseExpr();
    return s;
}

// ─── ASSIGN ──────────────────────────────────────────────────────────────────
StmtPtr Parser::parseAssign(const Token& idTok) {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::Assign;
    s->line = idTok.line; s->col = idTok.col;

    // Build target: could be var or arr[i]
    ExprPtr target = Expr::makeVar(idTok.value, idTok.line, idTok.col);
    if (check(TokenType::LBRACKET)) {
        advance(); // [
        auto idx = parseExpr();
        expect(TokenType::RBRACKET, "期望 ']'");
        target = Expr::makeIndex(std::move(target), std::move(idx), idTok.line, idTok.col);
    }
    s->target = std::move(target);
    expect(TokenType::ASSIGN, "期望 '←' 或 '<-'");
    s->value = parseExpr();
    return s;
}

// ─── OUTPUT ──────────────────────────────────────────────────────────────────
StmtPtr Parser::parseOutput() {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::Output;
    s->line = peek().line; s->col = peek().col;
    advance(); // OUTPUT or PRINT

    s->outputs.push_back(parseExpr());
    while (match(TokenType::COMMA))
        s->outputs.push_back(parseExpr());
    return s;
}

// ─── INPUT ───────────────────────────────────────────────────────────────────
StmtPtr Parser::parseInput() {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::Input;
    s->line = peek().line; s->col = peek().col;
    advance(); // INPUT

    Token var = expect(TokenType::IDENTIFIER, "期望变量名");
    s->inputVar = var.value;
    return s;
}

// ─── IF ──────────────────────────────────────────────────────────────────────
StmtPtr Parser::parseIf() {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::If;
    s->line = peek().line; s->col = peek().col;
    advance(); // IF

    s->condition = parseExpr();
    expect(TokenType::KW_THEN, "期望 THEN");

    s->thenBranch = parseBlock({TokenType::KW_ELSE, TokenType::KW_ENDIF});

    if (match(TokenType::KW_ELSE)) {
        s->elseBranch = parseBlock({TokenType::KW_ENDIF});
    }
    expect(TokenType::KW_ENDIF, "期望 ENDIF");
    return s;
}

// ─── FOR ─────────────────────────────────────────────────────────────────────
StmtPtr Parser::parseFor() {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::ForLoop;
    s->line = peek().line; s->col = peek().col;
    advance(); // FOR

    Token var = expect(TokenType::IDENTIFIER, "期望循环变量名");
    s->loopVar = var.value;
    expect(TokenType::ASSIGN, "期望 '←'");
    s->forFrom = parseExpr();
    expect(TokenType::KW_TO, "期望 TO");
    s->forTo = parseExpr();

    if (match(TokenType::KW_STEP))
        s->forStep = parseExpr();

    s->forBody = parseBlock({TokenType::KW_NEXT});

    expect(TokenType::KW_NEXT, "期望 NEXT");
    // Optionally consume the loop variable name after NEXT
    if (check(TokenType::IDENTIFIER) && peek().value == var.value)
        advance();
    return s;
}

// ─── WHILE ───────────────────────────────────────────────────────────────────
StmtPtr Parser::parseWhile() {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::WhileLoop;
    s->line = peek().line; s->col = peek().col;
    advance(); // WHILE

    s->condition = parseExpr();
    match(TokenType::KW_DO); // optional DO
    s->thenBranch = parseBlock({TokenType::KW_ENDWHILE});
    expect(TokenType::KW_ENDWHILE, "期望 ENDWHILE");
    return s;
}

// ─── REPEAT ──────────────────────────────────────────────────────────────────
StmtPtr Parser::parseRepeat() {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::RepeatLoop;
    s->line = peek().line; s->col = peek().col;
    advance(); // REPEAT

    s->thenBranch = parseBlock({TokenType::KW_UNTIL});
    expect(TokenType::KW_UNTIL, "期望 UNTIL");
    s->condition = parseExpr();
    return s;
}

// ─── CASE OF ─────────────────────────────────────────────────────────────────
StmtPtr Parser::parseCase() {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::CaseOf;
    s->line = peek().line; s->col = peek().col;
    advance(); // CASE
    expect(TokenType::KW_OF, "期望 OF 在 CASE 后");

    s->caseExpr = parseExpr();
    skipNewlines();

    while (!checkAny({TokenType::KW_ENDCASE, TokenType::END_OF_FILE})) {
        CaseBranch branch;
        if (match(TokenType::KW_OTHERWISE)) {
            branch.value = nullptr;
            match(TokenType::COLON); // optional colon after OTHERWISE
        } else {
            branch.value = parseExpr();
            expect(TokenType::COLON, "期望 ':' 在 CASE 分支后");
        }
        // Parse branch body: statements until next literal+colon, OTHERWISE, or ENDCASE
        auto branchBlock = std::make_unique<Stmt>();
        branchBlock->kind = StmtKind::Block;
        skipNewlines();
        while (!checkAny({TokenType::KW_OTHERWISE, TokenType::KW_ENDCASE, TokenType::END_OF_FILE})) {
            // Check if this looks like the start of a new CASE branch (literal followed by colon)
            bool nextBranch = false;
            if (checkAny({TokenType::INTEGER_LIT, TokenType::REAL_LIT,
                           TokenType::STRING_LIT, TokenType::CHAR_LIT,
                           TokenType::KW_TRUE, TokenType::KW_FALSE})) {
                size_t savedPos = pos_;
                advance(); // consume the literal
                if (check(TokenType::COLON)) nextBranch = true;
                pos_ = savedPos; // restore
            }
            if (nextBranch) break;
            try {
                auto st = parseStatement();
                if (st) branchBlock->body.push_back(std::move(st));
            } catch (...) { syncToNewline(); }
            skipNewlines();
        }
        branch.body = std::move(branchBlock);
        s->caseBranches.push_back(std::move(branch));
    }
    expect(TokenType::KW_ENDCASE, "期望 ENDCASE");
    return s;
}

// ─── CALL ────────────────────────────────────────────────────────────────────
StmtPtr Parser::parseCall() {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::ProcCall;
    s->line = peek().line; s->col = peek().col;
    advance(); // CALL

    Token name = expect(TokenType::IDENTIFIER, "期望过程名");
    s->callName = name.value;

    if (match(TokenType::LPAREN)) {
        if (!check(TokenType::RPAREN)) {
            s->callArgs.push_back(parseExpr());
            while (match(TokenType::COMMA))
                s->callArgs.push_back(parseExpr());
        }
        expect(TokenType::RPAREN, "期望 ')'");
    }
    return s;
}

// ─── RETURN ──────────────────────────────────────────────────────────────────
StmtPtr Parser::parseReturn() {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::Return;
    s->line = peek().line; s->col = peek().col;
    advance(); // RETURN

    if (!checkAny({TokenType::NEWLINE, TokenType::END_OF_FILE}))
        s->returnVal = parseExpr();
    return s;
}

// ─── FUNCTION ────────────────────────────────────────────────────────────────
StmtPtr Parser::parseFunction() {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::FunctionDef;
    s->line = peek().line; s->col = peek().col;
    advance(); // FUNCTION

    Token name = expect(TokenType::IDENTIFIER, "期望函数名");
    s->funcName = name.value;
    expect(TokenType::LPAREN, "期望 '('");
    s->params = parseParamList();
    expect(TokenType::RPAREN, "期望 ')'");
    expect(TokenType::KW_RETURNS, "期望 RETURNS");
    s->returnType = parseTypeSpec();

    s->funcBody = parseBlock({TokenType::KW_ENDFUNCTION});
    expect(TokenType::KW_ENDFUNCTION, "期望 ENDFUNCTION");
    return s;
}

// ─── PROCEDURE ───────────────────────────────────────────────────────────────
StmtPtr Parser::parseProcedure() {
    auto s = std::make_unique<Stmt>();
    s->kind = StmtKind::ProcedureDef;
    s->line = peek().line; s->col = peek().col;
    advance(); // PROCEDURE

    Token name = expect(TokenType::IDENTIFIER, "期望过程名");
    s->funcName = name.value;

    s->params.clear();
    if (match(TokenType::LPAREN)) {
        s->params = parseParamList();
        expect(TokenType::RPAREN, "期望 ')'");
    }

    s->funcBody = parseBlock({TokenType::KW_ENDPROCEDURE});
    expect(TokenType::KW_ENDPROCEDURE, "期望 ENDPROCEDURE");
    return s;
}

// ─── TypeSpec ─────────────────────────────────────────────────────────────────
TypeSpec Parser::parseTypeSpec() {
    TypeSpec ts;
    if (match(TokenType::KW_ARRAY)) {
        ts.isArray = true;
        expect(TokenType::LBRACKET, "期望 '[' 在数组类型后");
        // low bound
        if (check(TokenType::INTEGER_LIT)) {
            ts.arrayLow = std::stoi(advance().value);
        }
        expect(TokenType::COLON, "期望 ':' 在数组范围中");
        if (check(TokenType::INTEGER_LIT)) {
            ts.arrayHigh = std::stoi(advance().value);
        }
        expect(TokenType::RBRACKET, "期望 ']'");
        expect(TokenType::KW_OF, "期望 OF");
    }
    // base type
    if (check(TokenType::KW_INTEGER))      { ts.base = "INTEGER"; advance(); }
    else if (check(TokenType::KW_REAL))    { ts.base = "REAL"; advance(); }
    else if (check(TokenType::KW_STRING))  { ts.base = "STRING"; advance(); }
    else if (check(TokenType::KW_BOOLEAN)) { ts.base = "BOOLEAN"; advance(); }
    else if (check(TokenType::KW_CHAR))    { ts.base = "CHAR"; advance(); }
    else {
        diag_.error(peek().line, peek().col, "期望类型名（INTEGER/REAL/STRING/BOOLEAN/CHAR）");
        ts.base = "INTEGER";
    }
    return ts;
}

// ─── ParamList ────────────────────────────────────────────────────────────────
std::vector<Param> Parser::parseParamList() {
    std::vector<Param> params;
    if (check(TokenType::RPAREN)) return params;

    auto parseOne = [&]() {
        Param p;
        if (match(TokenType::KW_BYREF))       p.byRef = true;
        else match(TokenType::KW_BYVALUE);     // optional, default

        Token name = expect(TokenType::IDENTIFIER, "期望参数名");
        p.name = name.value;
        expect(TokenType::COLON, "期望 ':'");
        p.type = parseTypeSpec();
        return p;
    };

    params.push_back(parseOne());
    while (match(TokenType::COMMA))
        params.push_back(parseOne());
    return params;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  EXPRESSIONS (Pratt-style recursive descent)
// ═══════════════════════════════════════════════════════════════════════════════
ExprPtr Parser::parseExpr() { return parseOr(); }

ExprPtr Parser::parseOr() {
    auto left = parseAnd();
    while (check(TokenType::KW_OR)) {
        int l = peek().line, c = peek().col;
        advance();
        auto right = parseAnd();
        left = Expr::makeBinOp("OR", std::move(left), std::move(right), l, c);
    }
    return left;
}

ExprPtr Parser::parseAnd() {
    auto left = parseNot();
    while (check(TokenType::KW_AND)) {
        int l = peek().line, c = peek().col;
        advance();
        auto right = parseNot();
        left = Expr::makeBinOp("AND", std::move(left), std::move(right), l, c);
    }
    return left;
}

ExprPtr Parser::parseNot() {
    if (check(TokenType::KW_NOT)) {
        int l = peek().line, c = peek().col;
        advance();
        auto operand = parseNot();
        return Expr::makeUnary("NOT", std::move(operand), l, c);
    }
    return parseComparison();
}

ExprPtr Parser::parseComparison() {
    auto left = parseConcat();
    while (checkAny({TokenType::EQ, TokenType::NEQ, TokenType::LT,
                     TokenType::GT, TokenType::LE, TokenType::GE})) {
        int l = peek().line, c = peek().col;
        std::string op = advance().value;
        auto right = parseConcat();
        left = Expr::makeBinOp(op, std::move(left), std::move(right), l, c);
    }
    return left;
}

ExprPtr Parser::parseConcat() {
    auto left = parseAddSub();
    while (check(TokenType::AMPERSAND)) {
        int l = peek().line, c = peek().col;
        advance();
        auto right = parseAddSub();
        left = Expr::makeBinOp("&", std::move(left), std::move(right), l, c);
    }
    return left;
}

ExprPtr Parser::parseAddSub() {
    auto left = parseMulDiv();
    while (checkAny({TokenType::PLUS, TokenType::MINUS})) {
        int l = peek().line, c = peek().col;
        std::string op = advance().value;
        auto right = parseMulDiv();
        left = Expr::makeBinOp(op, std::move(left), std::move(right), l, c);
    }
    return left;
}

ExprPtr Parser::parseMulDiv() {
    auto left = parsePower();
    while (checkAny({TokenType::STAR, TokenType::SLASH,
                     TokenType::KW_DIV, TokenType::KW_MOD})) {
        int l = peek().line, c = peek().col;
        std::string op = advance().value;
        auto right = parsePower();
        left = Expr::makeBinOp(op, std::move(left), std::move(right), l, c);
    }
    return left;
}

ExprPtr Parser::parsePower() {
    auto left = parseUnary();
    if (check(TokenType::CARET)) {
        int l = peek().line, c = peek().col;
        advance();
        auto right = parsePower(); // right-associative
        return Expr::makeBinOp("^", std::move(left), std::move(right), l, c);
    }
    return left;
}

ExprPtr Parser::parseUnary() {
    if (check(TokenType::MINUS)) {
        int l = peek().line, c = peek().col;
        advance();
        auto operand = parseUnary();
        return Expr::makeUnary("-", std::move(operand), l, c);
    }
    if (check(TokenType::PLUS)) {
        advance();
        return parseUnary();
    }
    return parsePostfix();
}

ExprPtr Parser::parsePostfix() {
    auto base = parsePrimary();
    while (check(TokenType::LBRACKET)) {
        int l = peek().line, c = peek().col;
        advance(); // [
        auto idx = parseExpr();
        expect(TokenType::RBRACKET, "期望 ']'");
        base = Expr::makeIndex(std::move(base), std::move(idx), l, c);
    }
    return base;
}

ExprPtr Parser::parsePrimary() {
    const Token& t = peek();

    if (check(TokenType::INTEGER_LIT)) {
        advance();
        return Expr::makeIntLit(std::stoll(t.value), t.line, t.col);
    }
    if (check(TokenType::REAL_LIT)) {
        advance();
        return Expr::makeRealLit(std::stod(t.value), t.line, t.col);
    }
    if (check(TokenType::STRING_LIT)) {
        advance();
        return Expr::makeStrLit(t.value, t.line, t.col);
    }
    if (check(TokenType::CHAR_LIT)) {
        advance();
        return Expr::makeCharLit(t.value.empty() ? '\0' : t.value[0], t.line, t.col);
    }
    if (check(TokenType::KW_TRUE)) {
        advance();
        return Expr::makeBoolLit(true, t.line, t.col);
    }
    if (check(TokenType::KW_FALSE)) {
        advance();
        return Expr::makeBoolLit(false, t.line, t.col);
    }
    if (check(TokenType::IDENTIFIER)) {
        advance();
        // Function call as expression
        if (check(TokenType::LPAREN)) {
            advance();
            std::vector<ExprPtr> args;
            if (!check(TokenType::RPAREN)) {
                args.push_back(parseExpr());
                while (match(TokenType::COMMA))
                    args.push_back(parseExpr());
            }
            expect(TokenType::RPAREN, "期望 ')'");
            return Expr::makeCall(t.value, std::move(args), t.line, t.col);
        }
        return Expr::makeVar(t.value, t.line, t.col);
    }
    if (check(TokenType::LPAREN)) {
        advance();
        auto e = parseExpr();
        expect(TokenType::RPAREN, "期望 ')'");
        return e;
    }

    diag_.error(t.line, t.col,
        "期望表达式，但遇到了 " + t.typeName());
    advance(); // skip to prevent infinite loop
    return Expr::makeIntLit(0, t.line, t.col); // sentinel
}
