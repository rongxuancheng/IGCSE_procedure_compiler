#pragma once
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include "Lexer.h"

// ─── Forward declarations ─────────────────────────────────────────────────────
struct Expr;
struct Stmt;
using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

// ─── Type representation ──────────────────────────────────────────────────────
struct TypeSpec {
    std::string base;           // INTEGER, REAL, STRING, BOOLEAN, CHAR
    bool        isArray = false;
    int         arrayLow  = 1;
    int         arrayHigh = 1;
};

// ═══════════════════════════════════════════════════════════════════════════════
//  EXPRESSIONS
// ═══════════════════════════════════════════════════════════════════════════════
enum class ExprKind {
    IntLit, RealLit, StrLit, BoolLit, CharLit,
    Var, ArrayIndex,
    BinOp, UnaryOp,
    Call,    // function call as expression
};

struct Expr {
    ExprKind    kind;
    int line = 0, col = 0;

    // Literal values
    long long   intVal  = 0;
    double      realVal = 0.0;
    std::string strVal;
    bool        boolVal = false;
    char        charVal = 0;

    // Variable / call name
    std::string name;

    // BinOp / UnaryOp
    std::string          op;
    std::vector<ExprPtr> children;  // [0]=left,[1]=right for binop; [0] for unary

    // ArrayIndex: children[0]=base expr (var), children[1]=index expr
    // Call: children = arguments

    static ExprPtr makeIntLit(long long v, int line, int col);
    static ExprPtr makeRealLit(double v, int line, int col);
    static ExprPtr makeStrLit(const std::string& v, int line, int col);
    static ExprPtr makeBoolLit(bool v, int line, int col);
    static ExprPtr makeCharLit(char v, int line, int col);
    static ExprPtr makeVar(const std::string& name, int line, int col);
    static ExprPtr makeBinOp(const std::string& op, ExprPtr l, ExprPtr r, int line, int col);
    static ExprPtr makeUnary(const std::string& op, ExprPtr operand, int line, int col);
    static ExprPtr makeCall(const std::string& name, std::vector<ExprPtr> args, int line, int col);
    static ExprPtr makeIndex(ExprPtr arr, ExprPtr idx, int line, int col);
};

// ═══════════════════════════════════════════════════════════════════════════════
//  STATEMENTS
// ═══════════════════════════════════════════════════════════════════════════════
enum class StmtKind {
    Block,
    Declare, Constant,
    Assign,
    Output,
    Input,
    If,
    ForLoop, WhileLoop, RepeatLoop,
    CaseOf,
    ProcCall,
    Return,
    FunctionDef, ProcedureDef,
};

struct Param {
    std::string name;
    TypeSpec    type;
    bool        byRef = false;
};

struct CaseBranch {
    ExprPtr  value;   // nullptr = OTHERWISE
    StmtPtr  body;
};

struct Stmt {
    StmtKind kind;
    int line = 0, col = 0;

    // Block
    std::vector<StmtPtr> body;

    // Declare / Constant
    std::string name;
    TypeSpec    typeSpec;
    ExprPtr     initExpr;   // for CONSTANT

    // Assign
    ExprPtr     target;     // can be Var or ArrayIndex
    ExprPtr     value;

    // Output
    std::vector<ExprPtr> outputs;

    // Input
    std::string inputVar;

    // If
    ExprPtr     condition;
    StmtPtr     thenBranch;
    StmtPtr     elseBranch;

    // ForLoop
    std::string loopVar;
    ExprPtr     forFrom, forTo, forStep;
    StmtPtr     forBody;

    // WhileLoop
    // condition + body already declared

    // RepeatLoop
    // body + condition

    // CaseOf
    ExprPtr                  caseExpr;
    std::vector<CaseBranch>  caseBranches;

    // ProcCall
    std::string              callName;
    std::vector<ExprPtr>     callArgs;

    // Return
    ExprPtr returnVal;

    // FunctionDef / ProcedureDef
    std::string         funcName;
    std::vector<Param>  params;
    TypeSpec            returnType;  // only for FUNCTION
    StmtPtr             funcBody;
};
