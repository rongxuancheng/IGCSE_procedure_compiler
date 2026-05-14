#pragma once
#include "AST.h"
#include "Lexer.h"
#include <string>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <set>

// ─── Symbol Table ─────────────────────────────────────────────────────────────
struct Symbol {
    std::string name;
    TypeSpec    type;
    bool        isConst   = false;
    bool        isParam   = false;
    bool        byRef     = false;
};

class SymbolTable {
public:
    void pushScope();
    void popScope();
    bool declare(const Symbol& sym);     // false if already declared in current scope
    Symbol* lookup(const std::string& name);
    bool inCurrentScope(const std::string& name) const;

private:
    std::vector<std::unordered_map<std::string, Symbol>> scopes_;
};

// ─── Code Generator ───────────────────────────────────────────────────────────
class CodeGen {
public:
    CodeGen(DiagEngine& diag);

    std::string generate(Stmt* program);   // returns full C++ source

private:
    DiagEngine&   diag_;
    SymbolTable   symTable_;
    std::ostringstream  hdr_;   // includes / forward decls
    std::ostringstream  out_;   // main body
    std::ostringstream  funcs_; // generated functions
    int           indent_ = 0;
    bool          inFunction_ = false;
    std::string   currentReturnType_;

    // Set of builtin functions to emit helpers for
    std::set<std::string> usedBuiltins_;

    // ── Emit helpers ───────────────────────────────────────────────────
    void emit(const std::string& s);
    void emitLine(const std::string& s = "");
    std::string ind() const;

    // ── Statement codegen ──────────────────────────────────────────────
    void genStmt(Stmt* s);
    void genBlock(Stmt* s);
    void genDeclare(Stmt* s);
    void genConstant(Stmt* s);
    void genAssign(Stmt* s);
    void genOutput(Stmt* s);
    void genInput(Stmt* s);
    void genIf(Stmt* s);
    void genFor(Stmt* s);
    void genWhile(Stmt* s);
    void genRepeat(Stmt* s);
    void genCase(Stmt* s);
    void genProcCall(Stmt* s);
    void genReturn(Stmt* s);
    void genFunction(Stmt* s);
    void genProcedure(Stmt* s);

    // ── Expression codegen ─────────────────────────────────────────────
    std::string genExpr(Expr* e);
    std::string genBinOp(Expr* e);

    // ── Type helpers ───────────────────────────────────────────────────
    std::string toCppType(const TypeSpec& t);
    std::string defaultValue(const TypeSpec& t);

    // ── Builtin helpers ────────────────────────────────────────────────
    std::string tryBuiltin(const std::string& name, const std::vector<ExprPtr>& args);
    void emitBuiltinHelpers();

    // ── Semantic checks ────────────────────────────────────────────────
    bool checkAssignable(Expr* target);
};
