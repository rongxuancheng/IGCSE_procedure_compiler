#include "CodeGen.h"
#include <sstream>
#include <stdexcept>
#include <cmath>
#include <algorithm>

// ─── SymbolTable ─────────────────────────────────────────────────────────────
void SymbolTable::pushScope() { scopes_.push_back({}); }
void SymbolTable::popScope()  { if (!scopes_.empty()) scopes_.pop_back(); }

bool SymbolTable::declare(const Symbol& sym) {
    if (scopes_.empty()) scopes_.push_back({});
    auto& top = scopes_.back();
    if (top.count(sym.name)) return false;
    top[sym.name] = sym;
    return true;
}

Symbol* SymbolTable::lookup(const std::string& name) {
    for (int i = (int)scopes_.size() - 1; i >= 0; i--) {
        auto it = scopes_[i].find(name);
        if (it != scopes_[i].end()) return &it->second;
    }
    return nullptr;
}

bool SymbolTable::inCurrentScope(const std::string& name) const {
    if (scopes_.empty()) return false;
    return scopes_.back().count(name) > 0;
}

// ─── CodeGen ──────────────────────────────────────────────────────────────────
CodeGen::CodeGen(DiagEngine& diag) : diag_(diag) {}

std::string CodeGen::ind() const { return std::string(indent_ * 4, ' '); }

void CodeGen::emit(const std::string& s) { out_ << s; }
void CodeGen::emitLine(const std::string& s) { out_ << ind() << s << "\n"; }

// ─── Type helpers ─────────────────────────────────────────────────────────────
std::string CodeGen::toCppType(const TypeSpec& t) {
    std::string base;
    if      (t.base == "INTEGER") base = "long long";
    else if (t.base == "REAL")    base = "double";
    else if (t.base == "STRING")  base = "std::string";
    else if (t.base == "BOOLEAN") base = "bool";
    else if (t.base == "CHAR")    base = "char";
    else                          base = "long long";

    if (t.isArray) {
        int sz = t.arrayHigh - t.arrayLow + 1;
        return "std::vector<" + base + ">";
    }
    return base;
}

std::string CodeGen::defaultValue(const TypeSpec& t) {
    if (t.isArray) {
        int sz = t.arrayHigh - t.arrayLow + 1;
        std::string base;
        if      (t.base == "INTEGER") base = "0LL";
        else if (t.base == "REAL")    base = "0.0";
        else if (t.base == "BOOLEAN") base = "false";
        else if (t.base == "CHAR")    base = "'\\0'";
        else if (t.base == "STRING")  base = "\"\"";
        else base = "0LL";
        return std::to_string(sz) + ", " + base;
    }
    if (t.base == "INTEGER") return "0LL";
    if (t.base == "REAL")    return "0.0";
    if (t.base == "BOOLEAN") return "false";
    if (t.base == "CHAR")    return "'\\0'";
    if (t.base == "STRING")  return "\"\"";
    return "0";
}

// ─── Builtin functions ───────────────────────────────────────────────────────
std::string CodeGen::tryBuiltin(const std::string& name,
                                const std::vector<ExprPtr>& args) {
    std::string uname = name;
    std::transform(uname.begin(), uname.end(), uname.begin(), ::toupper);

    auto arg = [&](int i) -> std::string {
        if (i < (int)args.size()) return genExpr(args[i].get());
        return "0";
    };

    if (uname == "LENGTH" || uname == "LEN") {
        usedBuiltins_.insert("LENGTH");
        return "(long long)(" + arg(0) + ").length()";
    }
    if (uname == "UCASE")     return "_igcse_ucase(" + arg(0) + ")";
    if (uname == "LCASE")     return "_igcse_lcase(" + arg(0) + ")";
    if (uname == "SUBSTRING" || uname == "MID") {
        // SUBSTRING(str, start, len)  -- 1-based
        return "(" + arg(0) + ").substr((" + arg(1) + ")-1, " + arg(2) + ")";
    }
    if (uname == "INT")     return "(long long)(" + arg(0) + ")";
    if (uname == "REAL")    return "(double)(" + arg(0) + ")";
    if (uname == "STRING" || uname == "STR") {
        usedBuiltins_.insert("TO_STRING");
        return "_igcse_tostring(" + arg(0) + ")";
    }
    if (uname == "NUM_TO_STR" || uname == "NUMTOSTR") {
        usedBuiltins_.insert("TO_STRING");
        return "_igcse_tostring(" + arg(0) + ")";
    }
    if (uname == "STR_TO_NUM" || uname == "STRTONUM") {
        return "std::stod(" + arg(0) + ")";
    }
    if (uname == "ASC")     return "(long long)(" + arg(0) + ")[0]";
    if (uname == "CHR")     return "std::string(1,(char)(" + arg(0) + "))";
    if (uname == "SQRT")    return "std::sqrt((double)(" + arg(0) + "))";
    if (uname == "ABS")     return "std::abs(" + arg(0) + ")";
    if (uname == "ROUND")   return "std::round((double)(" + arg(0) + "))";
    if (uname == "RANDOM")  return "(double)rand()/(double)RAND_MAX";
    if (uname == "EOF")     return "(_igcse_eof())";
    return ""; // not a builtin
}

void CodeGen::emitBuiltinHelpers() {
    hdr_ << "// ── IGCSE Runtime Helpers ──────────────────────────────────\n";
    hdr_ << "#ifdef _WIN32\n#include <windows.h>\n#endif\n";
    hdr_ << "#include <iostream>\n#include <string>\n#include <vector>\n";
    hdr_ << "#include <cmath>\n#include <cstdlib>\n#include <algorithm>\n";
    hdr_ << "#include <sstream>\n#include <stdexcept>\n\n";

    hdr_ << "static std::string _igcse_ucase(std::string s) {\n";
    hdr_ << "    std::transform(s.begin(),s.end(),s.begin(),::toupper); return s; }\n";
    hdr_ << "static std::string _igcse_lcase(std::string s) {\n";
    hdr_ << "    std::transform(s.begin(),s.end(),s.begin(),::tolower); return s; }\n";
    hdr_ << "template<typename T>\n";
    hdr_ << "static std::string _igcse_tostring(T v) {\n";
    hdr_ << "    std::ostringstream oss; oss << v; return oss.str(); }\n";
    hdr_ << "static bool _igcse_eof() { return std::cin.eof(); }\n\n";
}

// ─── Main generate ────────────────────────────────────────────────────────────
std::string CodeGen::generate(Stmt* program) {
    symTable_.pushScope();

    // First pass: collect function/procedure defs
    if (program->kind == StmtKind::Block) {
        for (auto& s : program->body) {
            if (s && (s->kind == StmtKind::FunctionDef ||
                      s->kind == StmtKind::ProcedureDef)) {
                genStmt(s.get());
            }
        }
    }

    // Second pass: main body
    out_ << "int main() {\n";
    indent_ = 1;
    emitLine("#ifdef _WIN32");
    emitLine("    SetConsoleOutputCP(65001);");
    emitLine("    SetConsoleCP(65001);");
    emitLine("#endif");
    emitLine("srand((unsigned)time(nullptr));");

    if (program->kind == StmtKind::Block) {
        for (auto& s : program->body) {
            if (!s) continue;
            if (s->kind == StmtKind::FunctionDef ||
                s->kind == StmtKind::ProcedureDef) continue;
            genStmt(s.get());
        }
    }

    emitLine("return 0;");
    indent_ = 0;
    out_ << "}\n";

    symTable_.popScope();

    // Build final source
    std::ostringstream final_;
    emitBuiltinHelpers();
    final_ << hdr_.str();
    final_ << "// ── Forward declarations / Functions ─────────────────────\n";
    final_ << funcs_.str() << "\n";
    final_ << "// ── Main program ─────────────────────────────────────────\n";
    final_ << out_.str();
    return final_.str();
}

// ─── Statement dispatch ───────────────────────────────────────────────────────
void CodeGen::genStmt(Stmt* s) {
    if (!s) return;
    switch (s->kind) {
        case StmtKind::Block:       genBlock(s);     break;
        case StmtKind::Declare:     genDeclare(s);   break;
        case StmtKind::Constant:    genConstant(s);  break;
        case StmtKind::Assign:      genAssign(s);    break;
        case StmtKind::Output:      genOutput(s);    break;
        case StmtKind::Input:       genInput(s);     break;
        case StmtKind::If:          genIf(s);        break;
        case StmtKind::ForLoop:     genFor(s);       break;
        case StmtKind::WhileLoop:   genWhile(s);     break;
        case StmtKind::RepeatLoop:  genRepeat(s);    break;
        case StmtKind::CaseOf:      genCase(s);      break;
        case StmtKind::ProcCall:    genProcCall(s);  break;
        case StmtKind::Return:      genReturn(s);    break;
        case StmtKind::FunctionDef: genFunction(s);  break;
        case StmtKind::ProcedureDef:genProcedure(s); break;
    }
}

void CodeGen::genBlock(Stmt* s) {
    for (auto& child : s->body)
        genStmt(child.get());
}

void CodeGen::genDeclare(Stmt* s) {
    Symbol sym;
    sym.name = s->name;
    sym.type = s->typeSpec;
    if (!symTable_.declare(sym)) {
        diag_.warning(s->line, s->col,
            "变量 '" + s->name + "' 已在当前作用域中声明");
        return;
    }
    std::string ctype = toCppType(s->typeSpec);
    std::string defval = defaultValue(s->typeSpec);
    if (s->typeSpec.isArray) {
        emitLine(ctype + " " + s->name + "(" + defval + ");");
    } else {
        emitLine(ctype + " " + s->name + " = " + defval + ";");
    }
}

void CodeGen::genConstant(Stmt* s) {
    Symbol sym;
    sym.name = s->name;
    sym.isConst = true;
    // Infer type from expression (simple: check kind)
    if (s->initExpr) {
        if (s->initExpr->kind == ExprKind::IntLit)  sym.type.base = "INTEGER";
        else if (s->initExpr->kind == ExprKind::RealLit)  sym.type.base = "REAL";
        else if (s->initExpr->kind == ExprKind::StrLit)  sym.type.base = "STRING";
        else if (s->initExpr->kind == ExprKind::BoolLit) sym.type.base = "BOOLEAN";
        else sym.type.base = "INTEGER";
    }
    symTable_.declare(sym);
    std::string val = s->initExpr ? genExpr(s->initExpr.get()) : "0";
    emitLine("const " + toCppType(sym.type) + " " + s->name + " = " + val + ";");
}

void CodeGen::genAssign(Stmt* s) {
    std::string tgt = genExpr(s->target.get());
    std::string val = genExpr(s->value.get());

    // Check array bounds at runtime? Skip for now.
    emitLine(tgt + " = " + val + ";");
}

void CodeGen::genOutput(Stmt* s) {
    for (size_t i = 0; i < s->outputs.size(); i++) {
        std::string val = genExpr(s->outputs[i].get());
        if (i + 1 < s->outputs.size())
            emitLine("std::cout << " + val + " << \" \";");
        else
            emitLine("std::cout << " + val + " << \"\\n\";");
    }
}

void CodeGen::genInput(Stmt* s) {
    // Check if var exists
    Symbol* sym = symTable_.lookup(s->inputVar);
    if (!sym) {
        diag_.warning(s->line, s->col,
            "INPUT：变量 '" + s->inputVar + "' 未声明，将隐式声明为 STRING");
        Symbol ns; ns.name = s->inputVar; ns.type.base = "STRING";
        symTable_.declare(ns);
        emitLine("std::string " + s->inputVar + ";");
    }
    emitLine("std::cin >> " + s->inputVar + ";");
}

void CodeGen::genIf(Stmt* s) {
    std::string cond = genExpr(s->condition.get());
    emitLine("if (" + cond + ") {");
    indent_++;
    genStmt(s->thenBranch.get());
    indent_--;
    if (s->elseBranch) {
        emitLine("} else {");
        indent_++;
        genStmt(s->elseBranch.get());
        indent_--;
    }
    emitLine("}");
}

void CodeGen::genFor(Stmt* s) {
    std::string from = genExpr(s->forFrom.get());
    std::string to   = genExpr(s->forTo.get());
    std::string step = s->forStep ? genExpr(s->forStep.get()) : "1";

    // Declare loop var if not already declared
    Symbol* sym = symTable_.lookup(s->loopVar);
    std::string decl;
    if (!sym) {
        Symbol ns; ns.name = s->loopVar; ns.type.base = "INTEGER";
        symTable_.declare(ns);
        decl = "long long ";
    }

    emitLine("{");
    indent_++;
    emitLine("long long _step_" + s->loopVar + " = " + step + ";");
    emitLine("for (" + decl + s->loopVar + " = " + from + "; "
             "_step_" + s->loopVar + " > 0 ? "
             + s->loopVar + " <= " + to + " : "
             + s->loopVar + " >= " + to + "; "
             + s->loopVar + " += _step_" + s->loopVar + ") {");
    indent_++;
    genStmt(s->forBody.get());
    indent_--;
    emitLine("}");
    indent_--;
    emitLine("}");
}

void CodeGen::genWhile(Stmt* s) {
    std::string cond = genExpr(s->condition.get());
    emitLine("while (" + cond + ") {");
    indent_++;
    genStmt(s->thenBranch.get());
    indent_--;
    emitLine("}");
}

void CodeGen::genRepeat(Stmt* s) {
    emitLine("do {");
    indent_++;
    genStmt(s->thenBranch.get());
    indent_--;
    std::string cond = genExpr(s->condition.get());
    emitLine("} while (!(" + cond + "));");
}

void CodeGen::genCase(Stmt* s) {
    std::string expr = genExpr(s->caseExpr.get());
    std::string tmp = "_case_val";
    emitLine("{");
    indent_++;
    emitLine("auto " + tmp + " = " + expr + ";");
    bool first = true;
    for (auto& br : s->caseBranches) {
        if (!br.value) {
            // OTHERWISE
            emitLine("} else {");
        } else {
            std::string val = genExpr(br.value.get());
            if (first) {
                emitLine("if (" + tmp + " == " + val + ") {");
                first = false;
            } else {
                emitLine("} else if (" + tmp + " == " + val + ") {");
            }
        }
        indent_++;
        genStmt(br.body.get());
        indent_--;
    }
    if (!first) emitLine("}");
    indent_--;
    emitLine("}");
}

void CodeGen::genProcCall(Stmt* s) {
    // Check if builtin first (unlikely as statement but handle)
    std::string args_str;
    for (size_t i = 0; i < s->callArgs.size(); i++) {
        if (i) args_str += ", ";
        args_str += genExpr(s->callArgs[i].get());
    }
    emitLine(s->callName + "(" + args_str + ");");
}

void CodeGen::genReturn(Stmt* s) {
    if (s->returnVal) {
        emitLine("return " + genExpr(s->returnVal.get()) + ";");
    } else {
        emitLine("return;");
    }
}

void CodeGen::genFunction(Stmt* s) {
    // Generate into funcs_ buffer
    auto* savedOut = &out_;
    std::ostringstream* target = &funcs_;

    symTable_.pushScope();
    inFunction_ = true;

    std::string retType = toCppType(s->returnType);
    currentReturnType_ = retType;

    std::string sig = retType + " " + s->funcName + "(";
    for (size_t i = 0; i < s->params.size(); i++) {
        if (i) sig += ", ";
        const auto& p = s->params[i];
        std::string pt = toCppType(p.type);
        if (p.byRef) sig += pt + "& " + p.name;
        else         sig += pt + " " + p.name;

        Symbol sym; sym.name = p.name; sym.type = p.type;
        sym.isParam = true; sym.byRef = p.byRef;
        symTable_.declare(sym);
    }
    sig += ")";

    // redirect emit to funcs_
    auto prevOut = out_.str();
    out_.str(""); out_.clear();

    emitLine(sig + " {");
    indent_ = 1;
    genStmt(s->funcBody.get());
    // default return
    emitLine("return " + defaultValue(s->returnType) + "; // default");
    indent_ = 0;
    emitLine("}");

    funcs_ << out_.str();
    out_.str(""); out_.clear();
    out_ << prevOut;
    indent_ = 1;

    symTable_.popScope();
    inFunction_ = false;
}

void CodeGen::genProcedure(Stmt* s) {
    symTable_.pushScope();
    inFunction_ = true;

    std::string sig = "void " + s->funcName + "(";
    for (size_t i = 0; i < s->params.size(); i++) {
        if (i) sig += ", ";
        const auto& p = s->params[i];
        std::string pt = toCppType(p.type);
        if (p.byRef) sig += pt + "& " + p.name;
        else         sig += pt + " " + p.name;

        Symbol sym; sym.name = p.name; sym.type = p.type;
        sym.isParam = true; sym.byRef = p.byRef;
        symTable_.declare(sym);
    }
    sig += ")";

    auto prevOut = out_.str();
    out_.str(""); out_.clear();

    emitLine(sig + " {");
    indent_ = 1;
    genStmt(s->funcBody.get());
    indent_ = 0;
    emitLine("}");

    funcs_ << out_.str();
    out_.str(""); out_.clear();
    out_ << prevOut;
    indent_ = 1;

    symTable_.popScope();
    inFunction_ = false;
}

// ─── Expression codegen ───────────────────────────────────────────────────────
std::string CodeGen::genExpr(Expr* e) {
    if (!e) return "0";
    switch (e->kind) {
        case ExprKind::IntLit:   return std::to_string(e->intVal) + "LL";
        case ExprKind::RealLit: {
            std::ostringstream oss;
            oss << e->realVal;
            std::string s = oss.str();
            if (s.find('.') == std::string::npos) s += ".0";
            return s;
        }
        case ExprKind::StrLit: {
            // Escape the string for C++
            std::string out = "std::string(\"";
            for (char c : e->strVal) {
                if      (c == '"')  out += "\\\"";
                else if (c == '\\') out += "\\\\";
                else if (c == '\n') out += "\\n";
                else if (c == '\t') out += "\\t";
                else                out += c;
            }
            out += "\")";
            return out;
        }
        case ExprKind::BoolLit:  return e->boolVal ? "true" : "false";
        case ExprKind::CharLit: {
            std::string out = "'";
            char c = e->charVal;
            if (c == '\'') out += "\\'";
            else if (c == '\\') out += "\\\\";
            else out += c;
            out += "'";
            return out;
        }
        case ExprKind::Var: {
            Symbol* sym = symTable_.lookup(e->name);
            if (!sym) {
                diag_.error(e->line, e->col,
                    "未声明的变量 '" + e->name + "'");
                return "0";
            }
            return e->name;
        }
        case ExprKind::ArrayIndex: {
            std::string arr = genExpr(e->children[0].get());
            std::string idx = genExpr(e->children[1].get());

            // Look up low bound
            std::string name = e->children[0]->name;
            Symbol* sym = symTable_.lookup(name);
            int low = 1;
            if (sym) low = sym->type.arrayLow;

            return arr + ".at((" + idx + ") - " + std::to_string(low) + "LL)";
        }
        case ExprKind::BinOp:   return genBinOp(e);
        case ExprKind::UnaryOp: {
            std::string operand = genExpr(e->children[0].get());
            if (e->op == "-")   return "(-(" + operand + "))";
            if (e->op == "NOT") return "(!(" + operand + "))";
            return operand;
        }
        case ExprKind::Call: {
            // Try builtin first
            std::string builtin = tryBuiltin(e->name, e->children);
            if (!builtin.empty()) return builtin;

            // User-defined function call
            std::string args;
            for (size_t i = 0; i < e->children.size(); i++) {
                if (i) args += ", ";
                args += genExpr(e->children[i].get());
            }
            return e->name + "(" + args + ")";
        }
    }
    return "0";
}

std::string CodeGen::genBinOp(Expr* e) {
    std::string l = genExpr(e->children[0].get());
    std::string r = genExpr(e->children[1].get());
    const std::string& op = e->op;

    if (op == "+")   return "(" + l + " + " + r + ")";
    if (op == "-")   return "(" + l + " - " + r + ")";
    if (op == "*")   return "(" + l + " * " + r + ")";
    if (op == "/")   return "((double)" + l + " / (double)" + r + ")";
    if (op == "DIV") return "((long long)" + l + " / (long long)" + r + ")";
    if (op == "MOD") return "((long long)" + l + " % (long long)" + r + ")";
    if (op == "^")   return "std::pow((double)(" + l + "),(double)(" + r + "))";
    if (op == "&")   return "(_igcse_tostring(" + l + ") + _igcse_tostring(" + r + "))";
    if (op == "=")   return "(" + l + " == " + r + ")";
    if (op == "<>")  return "(" + l + " != " + r + ")";
    if (op == "<")   return "(" + l + " < " + r + ")";
    if (op == ">")   return "(" + l + " > " + r + ")";
    if (op == "<=")  return "(" + l + " <= " + r + ")";
    if (op == ">=")  return "(" + l + " >= " + r + ")";
    if (op == "AND") return "(" + l + " && " + r + ")";
    if (op == "OR")  return "(" + l + " || " + r + ")";
    return "(" + l + " " + op + " " + r + ")";
}
