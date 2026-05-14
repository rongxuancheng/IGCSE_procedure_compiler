#include "AST.h"

ExprPtr Expr::makeIntLit(long long v, int line, int col) {
    auto e = std::make_unique<Expr>();
    e->kind = ExprKind::IntLit; e->intVal = v;
    e->line = line; e->col = col;
    return e;
}
ExprPtr Expr::makeRealLit(double v, int line, int col) {
    auto e = std::make_unique<Expr>();
    e->kind = ExprKind::RealLit; e->realVal = v;
    e->line = line; e->col = col;
    return e;
}
ExprPtr Expr::makeStrLit(const std::string& v, int line, int col) {
    auto e = std::make_unique<Expr>();
    e->kind = ExprKind::StrLit; e->strVal = v;
    e->line = line; e->col = col;
    return e;
}
ExprPtr Expr::makeBoolLit(bool v, int line, int col) {
    auto e = std::make_unique<Expr>();
    e->kind = ExprKind::BoolLit; e->boolVal = v;
    e->line = line; e->col = col;
    return e;
}
ExprPtr Expr::makeCharLit(char v, int line, int col) {
    auto e = std::make_unique<Expr>();
    e->kind = ExprKind::CharLit; e->charVal = v;
    e->line = line; e->col = col;
    return e;
}
ExprPtr Expr::makeVar(const std::string& name, int line, int col) {
    auto e = std::make_unique<Expr>();
    e->kind = ExprKind::Var; e->name = name;
    e->line = line; e->col = col;
    return e;
}
ExprPtr Expr::makeBinOp(const std::string& op, ExprPtr l, ExprPtr r, int line, int col) {
    auto e = std::make_unique<Expr>();
    e->kind = ExprKind::BinOp; e->op = op;
    e->children.push_back(std::move(l));
    e->children.push_back(std::move(r));
    e->line = line; e->col = col;
    return e;
}
ExprPtr Expr::makeUnary(const std::string& op, ExprPtr operand, int line, int col) {
    auto e = std::make_unique<Expr>();
    e->kind = ExprKind::UnaryOp; e->op = op;
    e->children.push_back(std::move(operand));
    e->line = line; e->col = col;
    return e;
}
ExprPtr Expr::makeCall(const std::string& name, std::vector<ExprPtr> args, int line, int col) {
    auto e = std::make_unique<Expr>();
    e->kind = ExprKind::Call; e->name = name;
    e->children = std::move(args);
    e->line = line; e->col = col;
    return e;
}
ExprPtr Expr::makeIndex(ExprPtr arr, ExprPtr idx, int line, int col) {
    auto e = std::make_unique<Expr>();
    e->kind = ExprKind::ArrayIndex;
    e->children.push_back(std::move(arr));
    e->children.push_back(std::move(idx));
    e->line = line; e->col = col;
    return e;
}
