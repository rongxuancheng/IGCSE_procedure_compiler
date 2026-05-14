"""
IGCSE Pseudocode IDE
====================
纯 Python + tkinter 实现，Windows 原生运行，无需 WSL。
内置完整的词法分析、语法分析、C++ 代码生成器，
调用系统 g++（MSYS2/MinGW）编译并运行。
"""

import tkinter as tk
from tkinter import ttk, filedialog, messagebox, font
import subprocess, threading, os, sys, tempfile, json, re, time
from pathlib import Path

# ══════════════════════════════════════════════════════════════════════════════
#   THEME  —  深色 IDE 主题（类 VS Code Dark+）
# ══════════════════════════════════════════════════════════════════════════════
THEME = {
    "bg":           "#1e1e1e",
    "bg2":          "#252526",
    "bg3":          "#2d2d30",
    "border":       "#3e3e42",
    "fg":           "#d4d4d4",
    "fg_dim":       "#858585",
    "accent":       "#569cd6",
    "accent2":      "#4ec9b0",
    "green":        "#4ec9b0",
    "yellow":       "#dcdcaa",
    "orange":       "#ce9178",
    "red":          "#f44747",
    "purple":       "#c586c0",
    "string_col":   "#ce9178",
    "comment_col":  "#6a9955",
    "keyword_col":  "#569cd6",
    "number_col":   "#b5cea8",
    "type_col":     "#4ec9b0",
    "builtin_col":  "#dcdcaa",
    "sel_bg":       "#264f78",
    "line_bg":      "#2a2d2e",
    "gutter_bg":    "#1e1e1e",
    "gutter_fg":    "#858585",
    "status_bg":    "#007acc",
    "tab_active":   "#1e1e1e",
    "tab_inactive": "#2d2d30",
    "error_bg":     "#5a1d1d",
    "warn_bg":      "#3c3800",
}

# ══════════════════════════════════════════════════════════════════════════════
#   IGCSE LEXER  (Python, for syntax highlighting & error checking)
# ══════════════════════════════════════════════════════════════════════════════
KEYWORDS = {
    "DECLARE","CONSTANT","INTEGER","REAL","STRING","BOOLEAN","CHAR",
    "ARRAY","OF","OUTPUT","PRINT","INPUT","IF","THEN","ELSE","ENDIF",
    "FOR","TO","STEP","NEXT","WHILE","DO","ENDWHILE","REPEAT","UNTIL",
    "CASE","OTHERWISE","ENDCASE","FUNCTION","PROCEDURE","RETURNS","RETURN",
    "ENDFUNCTION","ENDPROCEDURE","CALL","BYVALUE","BYREF",
    "AND","OR","NOT","TRUE","FALSE","DIV","MOD",
}
TYPES    = {"INTEGER","REAL","STRING","BOOLEAN","CHAR","ARRAY"}
BUILTINS = {"LENGTH","LEN","UCASE","LCASE","SUBSTRING","MID","INT",
             "STR","STRING","NUM_TO_STR","STR_TO_NUM","ASC","CHR",
             "SQRT","ABS","ROUND","RANDOM","EOF"}

def tokenize_for_highlight(line: str):
    """
    Yields (start_col, end_col, tag) tuples for one source line.
    """
    tokens = []
    i = 0
    n = len(line)
    while i < n:
        # comment
        if line[i:i+2] == "//":
            tokens.append((i, n, "comment"))
            break
        # string
        if line[i] == '"':
            j = i + 1
            while j < n and line[j] != '"':
                j += 1
            tokens.append((i, j+1, "string"))
            i = j + 1
            continue
        # char
        if line[i] == "'":
            j = i + 1
            while j < n and line[j] != "'":
                j += 1
            tokens.append((i, j+1, "string"))
            i = j + 1
            continue
        # number
        if line[i].isdigit() or (line[i] == '.' and i+1 < n and line[i+1].isdigit()):
            j = i
            while j < n and (line[j].isdigit() or line[j] == '.'):
                j += 1
            tokens.append((i, j, "number"))
            i = j
            continue
        # identifier / keyword
        if line[i].isalpha() or line[i] == '_':
            j = i
            while j < n and (line[j].isalnum() or line[j] == '_'):
                j += 1
            word = line[i:j].upper()
            if word in TYPES and word in KEYWORDS:
                tokens.append((i, j, "type"))
            elif word in KEYWORDS:
                tokens.append((i, j, "keyword"))
            elif word in BUILTINS:
                tokens.append((i, j, "builtin"))
            else:
                tokens.append((i, j, "ident"))
            i = j
            continue
        # arrow ←
        if line[i:i+3] == "\xe2\x86\x90" or line[i:i+2] == "<-":
            w = 3 if line[i] == '\xe2' else 2
            tokens.append((i, i+w, "op"))
            i += w
            continue
        # operator / punct
        if line[i] in "+-*/^&=<>()[]:,":
            tokens.append((i, i+1, "op"))
        i += 1
    return tokens

# ══════════════════════════════════════════════════════════════════════════════
#   INLINE COMPILER  (Python port of the C++ compiler for instant feedback)
# ══════════════════════════════════════════════════════════════════════════════

class CompileError(Exception):
    def __init__(self, msg, line=0, col=0):
        super().__init__(msg)
        self.line = line
        self.col  = col

class Tok:
    __slots__ = ("type","val","line","col")
    def __init__(self, t, v, l, c):
        self.type=t; self.val=v; self.line=l; self.col=c
    def __repr__(self): return f"Tok({self.type},{self.val!r},{self.line}:{self.col})"

KW = set(k.upper() for k in KEYWORDS)

def lex(src: str):
    tokens = []
    lines  = src.split('\n')
    li = 0
    for raw_line in lines:
        li += 1
        i  = 0
        n  = len(raw_line)
        while i < n:
            c = raw_line[i]
            if c in ' \t\r':
                i += 1; continue
            if raw_line[i:i+2] == '//':
                break
            if c == '"':
                j = i+1
                while j < n and raw_line[j] != '"': j += 1
                tokens.append(Tok('STR', raw_line[i+1:j], li, i+1))
                i = j+1; continue
            if c == "'":
                j = i+1
                while j < n and raw_line[j] != "'": j += 1
                tokens.append(Tok('CHAR', raw_line[i+1:j], li, i+1))
                i = j+1; continue
            if c.isdigit() or (c=='.' and i+1<n and raw_line[i+1].isdigit()):
                j = i
                while j<n and (raw_line[j].isdigit() or raw_line[j]=='.'): j+=1
                t = 'REAL' if '.' in raw_line[i:j] else 'INT'
                tokens.append(Tok(t, raw_line[i:j], li, i+1))
                i=j; continue
            if c.isalpha() or c=='_':
                j=i
                while j<n and (raw_line[j].isalnum() or raw_line[j]=='_'): j+=1
                word=raw_line[i:j].upper()
                tokens.append(Tok(word if word in KW else 'ID', raw_line[i:j], li, i+1))
                i=j; continue
            # UTF-8 arrow ← (may appear as single char in Python str)
            if c == '←' or raw_line[i:i+2]=='<-':
                w = 2 if raw_line[i:i+2]=='<-' else 1
                tokens.append(Tok('ASSIGN','←',li,i+1))
                i+=w; continue
            two = raw_line[i:i+2]
            if two in ('<>','<=','>='):
                tokens.append(Tok(two,two,li,i+1)); i+=2; continue
            tokens.append(Tok(c,c,li,i+1)); i+=1
        tokens.append(Tok('NL','\n',li,n+1))
    tokens.append(Tok('EOF','',li+1,1))
    return tokens

# ── Minimal code generator (Python) ──────────────────────────────────────────
CPP_HEADER = r"""
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <sstream>
#include <ctime>
using namespace std;

static string _ts(long long v){ostringstream o;o<<v;return o.str();}
static string _ts(double v)   {ostringstream o;o<<v;return o.str();}
static string _ts(bool v)     {return v?"TRUE":"FALSE";}
static string _ts(char v)     {return string(1,v);}
static string _ts(string v)   {return v;}
template<typename T>
static string _ts(vector<T> v){return "[array]";}
static string _ucase(string s){transform(s.begin(),s.end(),s.begin(),::toupper);return s;}
static string _lcase(string s){transform(s.begin(),s.end(),s.begin(),::tolower);return s;}

"""

class PseudoCompiler:
    """
    Translates IGCSE pseudocode → C++ source string.
    Raises CompileError on syntax/semantic errors.
    """
    def __init__(self, src: str):
        self.tokens = lex(src)
        self.pos    = 0
        self.out    = []          # main body lines
        self.funcs  = []          # function/procedure definitions
        self.indent = 1
        self.errors = []

    # ── token helpers ─────────────────────────────────────────
    def peek(self, n=0):  return self.tokens[min(self.pos+n, len(self.tokens)-1)]
    def advance(self):
        t=self.tokens[self.pos]
        if self.pos+1<len(self.tokens): self.pos+=1
        return t
    def check(self,*ts):  return self.peek().type in ts
    def match(self,*ts):
        if self.check(*ts): return self.advance()
    def expect(self,t,msg):
        if self.check(t): return self.advance()
        p=self.peek()
        raise CompileError(f"{msg}，但遇到了 '{p.val}'", p.line, p.col)
    def skip_nl(self):
        while self.check('NL'): self.advance()

    # ── emit ──────────────────────────────────────────────────
    def emit(self, s): self.out.append("    "*self.indent + s)
    def emit_func(self,s): self.funcs.append(s)

    # ── type helpers ──────────────────────────────────────────
    def cpp_type(self, base, is_arr=False, lo=1, hi=1):
        m={"INTEGER":"long long","REAL":"double","STRING":"string",
           "BOOLEAN":"bool","CHAR":"char"}
        bt=m.get(base,"long long")
        if is_arr: return f"vector<{bt}>"
        return bt

    def default_val(self, base, is_arr=False, lo=1, hi=1):
        if is_arr:
            sz=hi-lo+1
            dv={"INTEGER":"0LL","REAL":"0.0","STRING":'""',"BOOLEAN":"false","CHAR":"'\\0'"}
            return f"{sz},{dv.get(base,'0LL')}"
        return {"INTEGER":"0LL","REAL":"0.0","STRING":'""',"BOOLEAN":"false","CHAR":"'\\0'"}.get(base,"0LL")

    def parse_type(self):
        is_arr=False; lo=1; hi=1
        if self.match('ARRAY'):
            is_arr=True
            self.expect('[','期望 [')
            lo=int(self.expect('INT','期望数组下界').val)
            self.expect(':','期望 :')
            hi=int(self.expect('INT','期望数组上界').val)
            self.expect(']','期望 ]')
            self.expect('OF','期望 OF')
        for bt in ('INTEGER','REAL','STRING','BOOLEAN','CHAR'):
            if self.match(bt): return bt,is_arr,lo,hi
        p=self.peek(); raise CompileError(f"期望类型名",p.line,p.col)

    # ── expression parser ─────────────────────────────────────
    def parse_expr(self): return self.parse_or()

    def parse_or(self):
        l=self.parse_and()
        while self.check('OR'):
            self.advance(); r=self.parse_and()
            l=f"({l}||{r})"
        return l

    def parse_and(self):
        l=self.parse_not()
        while self.check('AND'):
            self.advance(); r=self.parse_not()
            l=f"({l}&&{r})"
        return l

    def parse_not(self):
        if self.match('NOT'): return f"(!{self.parse_not()})"
        return self.parse_cmp()

    def parse_cmp(self):
        l=self.parse_cat()
        ops={'=':'==','<>':'!=','<':'<','>':'>','<=':'<=','>=':'>='}
        while self.peek().type in ops:
            op=self.advance().type; r=self.parse_cat()
            l=f"({l}{ops[op]}{r})"
        return l

    def parse_cat(self):
        l=self.parse_add()
        while self.check('&'):
            self.advance(); r=self.parse_add()
            l=f"(_ts({l})+_ts({r}))"
        return l

    def parse_add(self):
        l=self.parse_mul()
        while self.check('+','-'):
            op=self.advance().val; r=self.parse_mul()
            l=f"({l}{op}{r})"
        return l

    def parse_mul(self):
        l=self.parse_pow()
        while self.check('*','/','DIV','MOD'):
            op=self.advance()
            r=self.parse_pow()
            if op.type=='DIV': l=f"((long long){l}/(long long){r})"
            elif op.type=='MOD': l=f"((long long){l}%(long long){r})"
            elif op.val=='/': l=f"((double){l}/(double){r})"
            else: l=f"({l}*{r})"
        return l

    def parse_pow(self):
        b=self.parse_unary()
        if self.check('^'):
            self.advance(); e=self.parse_pow()
            return f"pow((double){b},(double){e})"
        return b

    def parse_unary(self):
        if self.check('-'): self.advance(); return f"(-{self.parse_unary()})"
        if self.check('+'): self.advance(); return self.parse_unary()
        return self.parse_postfix()

    def parse_postfix(self):
        base=self.parse_primary()
        while self.check('['):
            self.advance(); idx=self.parse_expr(); self.expect(']','期望 ]')
            base=f"{base}.at(({idx})-1)"  # simplified: assume 1-based
        return base

    def parse_primary(self):
        t=self.peek()
        if t.type=='INT':    self.advance(); return f"{t.val}LL"
        if t.type=='REAL':   self.advance(); return t.val
        if t.type=='STR':    self.advance(); return f'string("{self._esc(t.val)}")'
        if t.type=='CHAR':   self.advance(); v=t.val; return f"'{self._esc(v)}'"
        if t.type=='TRUE':   self.advance(); return "true"
        if t.type=='FALSE':  self.advance(); return "false"
        if t.type=='(':
            self.advance(); e=self.parse_expr(); self.expect(')','期望 )'); return f"({e})"
        if t.type=='ID':
            self.advance()
            name=t.val; uname=name.upper()
            if not self.check('('):
                if uname=='RANDOM': return "(double)rand()/(double)RAND_MAX"
                return name
            self.advance()
            if uname in ('LENGTH','LEN'):
                a=self.parse_expr(); self.expect(')','期望 )')
                return f"(long long)({a}).length()"
            if uname in ('UCASE','LCASE'):
                a=self.parse_expr(); self.expect(')','期望 )')
                return f"_{'ucase' if uname=='UCASE' else 'lcase'}({a})"
            if uname in ('SUBSTRING','MID'):
                s=self.parse_expr(); self.expect(',','期望 ,')
                st=self.parse_expr(); self.expect(',','期望 ,')
                ln=self.parse_expr(); self.expect(')','期望 )')
                return f"({s}).substr(({st})-1,{ln})"
            if uname=='INT':
                a=self.parse_expr(); self.expect(')','期望 )')
                return f"(long long)({a})"
            if uname=='SQRT':
                a=self.parse_expr(); self.expect(')','期望 )')
                return f"sqrt((double){a})"
            if uname=='ABS':
                a=self.parse_expr(); self.expect(')','期望 )')
                return f"abs({a})"
            if uname=='ROUND':
                a=self.parse_expr(); self.expect(')','期望 )')
                return f"(long long)round((double){a})"
            if uname in ('STR','NUM_TO_STR'):
                a=self.parse_expr(); self.expect(')','期望 )')
                return f"_ts({a})"
            if uname=='CHR':
                a=self.parse_expr(); self.expect(')','期望 )')
                return f"string(1,(char)({a}))"
            if uname=='ASC':
                a=self.parse_expr(); self.expect(')','期望 )')
                return f"(long long)({a})[0]"
            if uname=='RANDOM':
                self.expect(')','期望 )')
                return "(double)rand()/(double)RAND_MAX"
            args=[]
            if not self.check(')'):
                args.append(self.parse_expr())
                while self.match(','): args.append(self.parse_expr())
            self.expect(')','期望 )')
            return f"{name}({','.join(args)})"
            return name
        raise CompileError(f"期望表达式，遇到 '{t.val}'", t.line, t.col)

    def _esc(self,s):
        return s.replace('\\','\\\\').replace('"','\\"').replace("'",'\\\'').replace('\n','\\n')

    # ── statement parser ──────────────────────────────────────
    def parse_block(self, *stop):
        self.skip_nl()
        while not self.check(*stop,'EOF'):
            try:    self.parse_stmt()
            except CompileError as e:
                self.errors.append(e)
                # sync to next newline
                while not self.check('NL','EOF'): self.advance()
            self.skip_nl()

    def parse_stmt(self):
        self.skip_nl()
        t=self.peek()

        if t.type=='DECLARE':
            self.advance()
            name=self.expect('ID','期望变量名').val
            self.expect(':','期望 :')
            base,is_arr,lo,hi=self.parse_type()
            ct=self.cpp_type(base,is_arr,lo,hi)
            dv=self.default_val(base,is_arr,lo,hi)
            if is_arr: self.emit(f"{ct} {name}({dv});")
            else:       self.emit(f"{ct} {name}={dv};")

        elif t.type=='CONSTANT':
            self.advance()
            name=self.expect('ID','期望常量名').val
            self.expect('=','期望 =')
            val=self.parse_expr()
            self.emit(f"const auto {name}={val};")

        elif t.type in ('OUTPUT','PRINT'):
            self.advance()
            parts=[self.parse_expr()]
            while self.match(','): parts.append(self.parse_expr())
            inner=" << \" \" << ".join(f"_ts({p})" for p in parts)
            self.emit(f'cout << {inner} << "\\n";')

        elif t.type=='INPUT':
            self.advance()
            name=self.expect('ID','期望变量名').val
            if self.check('['):
                self.advance(); idx=self.parse_expr(); self.expect(']','期望 ]')
                self.emit(f"cin >> {name}.at(({idx})-1);")
            else:
                self.emit(f"cin >> {name};")

        elif t.type=='IF':
            self.advance()
            cond=self.parse_expr()
            self.expect('THEN','期望 THEN')
            self.emit(f"if({cond}){{")
            self.indent+=1
            self.parse_block('ELSE','ENDIF')
            self.indent-=1
            if self.match('ELSE'):
                self.emit("} else {")
                self.indent+=1
                self.parse_block('ENDIF')
                self.indent-=1
            self.expect('ENDIF','期望 ENDIF')
            self.emit("}")

        elif t.type=='FOR':
            self.advance()
            var=self.expect('ID','期望循环变量').val
            self.expect('ASSIGN','期望 ←')
            frm=self.parse_expr()
            self.expect('TO','期望 TO')
            to=self.parse_expr()
            step="1LL"
            if self.match('STEP'): step=self.parse_expr()
            self.emit(f"{{long long _stp={step};")
            self.emit(f"for(long long {var}={frm};_stp>0?{var}<={to}:{var}>={to};{var}+=_stp){{")
            self.indent+=1
            self.parse_block('NEXT')
            self.indent-=1
            self.expect('NEXT','期望 NEXT')
            self.match('ID')  # optional var name after NEXT
            self.emit("}}")

        elif t.type=='WHILE':
            self.advance()
            cond=self.parse_expr()
            self.match('DO')
            self.emit(f"while({cond}){{")
            self.indent+=1
            self.parse_block('ENDWHILE')
            self.indent-=1
            self.expect('ENDWHILE','期望 ENDWHILE')
            self.emit("}")

        elif t.type=='REPEAT':
            self.advance()
            self.emit("do{")
            self.indent+=1
            self.parse_block('UNTIL')
            self.indent-=1
            self.expect('UNTIL','期望 UNTIL')
            cond=self.parse_expr()
            self.emit(f"}}while(!({cond}));")

        elif t.type=='CASE':
            self.advance()
            self.expect('OF','期望 OF')
            expr=self.parse_expr()
            self.emit(f"{{auto _cv={expr};")
            self.skip_nl()
            first=True
            while not self.check('ENDCASE','EOF'):
                if self.match('OTHERWISE'):
                    self.match(':')
                    kw="} else {"
                else:
                    val=self.parse_expr()
                    self.expect(':','期望 :')
                    kw=f"if(_cv=={val}){{" if first else f"}}else if(_cv=={val}){{"
                    first=False
                self.emit(kw)
                self.indent+=1
                self.parse_block('CASE','OTHERWISE','ENDCASE','INT')
                self.indent-=1
                self.match('CASE')
                self.skip_nl()
            if not first: self.emit("}")
            self.expect('ENDCASE','期望 ENDCASE')
            self.emit("}")

        elif t.type=='RETURN':
            self.advance()
            if not self.check('NL','EOF'):
                val=self.parse_expr()
                self.emit(f"return {val};")
            else:
                self.emit("return;")

        elif t.type=='CALL':
            self.advance()
            name=self.expect('ID','期望过程名').val
            args=[]
            if self.match('('):
                if not self.check(')'): 
                    args.append(self.parse_expr())
                    while self.match(','): args.append(self.parse_expr())
                self.expect(')','期望 )')
            self.emit(f"{name}({','.join(args)});")

        elif t.type=='FUNCTION':
            self.advance()
            name=self.expect('ID','期望函数名').val
            self.expect('(','期望 (')
            params=self._parse_params()
            self.expect(')','期望 )')
            self.expect('RETURNS','期望 RETURNS')
            rbase,_,_,_=self.parse_type()
            rtype=self.cpp_type(rbase)
            sig=f"{rtype} {name}({params})"
            # redirect to funcs
            saved=self.out; self.out=[]; self.indent=1
            self.emit(sig+" {")
            self.indent=2
            self.parse_block('ENDFUNCTION')
            self.indent=1
            self.emit("}")
            self.funcs.extend(self.out)
            self.out=saved; self.indent=1
            self.expect('ENDFUNCTION','期望 ENDFUNCTION')

        elif t.type=='PROCEDURE':
            self.advance()
            name=self.expect('ID','期望过程名').val
            params=""
            if self.match('('): 
                params=self._parse_params()
                self.expect(')','期望 )')
            sig=f"void {name}({params})"
            saved=self.out; self.out=[]; self.indent=1
            self.emit(sig+" {")
            self.indent=2
            self.parse_block('ENDPROCEDURE')
            self.indent=1
            self.emit("}")
            self.funcs.extend(self.out)
            self.out=saved; self.indent=1
            self.expect('ENDPROCEDURE','期望 ENDPROCEDURE')

        elif t.type=='ID':
            self.advance(); name=t.val
            if self.check('ASSIGN'):
                self.advance(); val=self.parse_expr()
                self.emit(f"{name}={val};")
            elif self.check('['):
                self.advance(); idx=self.parse_expr(); self.expect(']','期望 ]')
                self.expect('ASSIGN','期望 ←')
                val=self.parse_expr()
                self.emit(f"{name}.at(({idx})-1)={val};")
            elif self.check('('):
                self.advance()
                args=[]
                if not self.check(')'): 
                    args.append(self.parse_expr())
                    while self.match(','): args.append(self.parse_expr())
                self.expect(')','期望 )')
                self.emit(f"{name}({','.join(args)});")
            else:
                raise CompileError(f"'{name}' 后期望 '←' 或 '('", t.line, t.col)

        elif t.type=='NL':
            self.advance()
        else:
            raise CompileError(f"意外的符号 '{t.val}'", t.line, t.col)

        # consume trailing newline
        self.match('NL')

    def _parse_params(self):
        parts=[]
        while not self.check(')','EOF','NL'):
            byref=False
            if self.match('BYREF'):   byref=True
            elif self.match('BYVALUE'): pass
            nm=self.expect('ID','期望参数名').val
            self.expect(':','期望 :')
            base,is_arr,lo,hi=self.parse_type()
            ct=self.cpp_type(base,is_arr,lo,hi)
            parts.append(f"{''+ct+'&' if byref else ct} {nm}")
            if not self.match(','): break
        return ",".join(parts)

    def compile(self):
        self.skip_nl()
        self.parse_block('EOF')
        if self.errors:
            raise self.errors[0]
        lines=["int main(){","    srand((unsigned)time(nullptr));"]
        lines+=self.out
        lines+=["    return 0;","}"]
        return CPP_HEADER + "\n".join(self.funcs) + "\n" + "\n".join(lines)


# ══════════════════════════════════════════════════════════════════════════════
#   IDE APPLICATION
# ══════════════════════════════════════════════════════════════════════════════

SAMPLE_CODE = """\
// IGCSE 伪代码示例 — 冒泡排序
DECLARE arr : ARRAY[1:5] OF INTEGER
DECLARE i   : INTEGER
DECLARE j   : INTEGER
DECLARE tmp : INTEGER
DECLARE n   : INTEGER

n ← 5
OUTPUT "请输入 5 个整数："

FOR i ← 1 TO n
    INPUT arr[i]
NEXT i

// 冒泡排序
FOR i ← 1 TO n - 1
    FOR j ← 1 TO n - i
        IF arr[j] > arr[j + 1] THEN
            tmp      ← arr[j]
            arr[j]   ← arr[j + 1]
            arr[j+1] ← tmp
        ENDIF
    NEXT j
NEXT i

OUTPUT "排序结果："
FOR i ← 1 TO n
    OUTPUT arr[i]
NEXT i
"""

IGCSE_KEYWORDS_HELP = [
    ("DECLARE",     "DECLARE <变量名> : <类型>"),
    ("CONSTANT",    "CONSTANT <名称> = <值>"),
    ("OUTPUT",      "OUTPUT <表达式> [, <表达式>...]"),
    ("INPUT",       "INPUT <变量名>"),
    ("IF",          "IF <条件> THEN ... [ELSE ...] ENDIF"),
    ("FOR",         "FOR <变量> ← <开始> TO <结束> [STEP <步长>] ... NEXT <变量>"),
    ("WHILE",       "WHILE <条件> DO ... ENDWHILE"),
    ("REPEAT",      "REPEAT ... UNTIL <条件>"),
    ("CASE OF",     "CASE OF <变量>\\n  <值> : ...\\n  OTHERWISE ...\\nENDCASE"),
    ("FUNCTION",    "FUNCTION <名>(<参数>) RETURNS <类型> ... ENDFUNCTION"),
    ("PROCEDURE",   "PROCEDURE <名>(<参数>) ... ENDPROCEDURE"),
    ("CALL",        "CALL <过程名>(<参数>)"),
    ("ARRAY",       "ARRAY[<低>:<高>] OF <类型>"),
    ("DIV",         "整数除法：a DIV b"),
    ("MOD",         "取余：a MOD b"),
]


class IGCSEide(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("IGCSE Pseudocode IDE")
        self.geometry("1280x800")
        self.configure(bg=THEME["bg"])
        self.minsize(900, 600)

        self._filepath  = None
        self._modified  = False
        self._proc      = None          # running subprocess
        self._hl_job    = None          # pending highlight after-id
        self._compiler_path = self._find_compiler()

        self._build_ui()
        self._bind_events()
        self._insert_sample()
        self.after(100, self._highlight_all)

    # ── compiler search ───────────────────────────────────────
    def _find_compiler(self):
        """Search for g++ in common Windows locations."""
        candidates = [
            r"C:\msys64\ucrt64\bin\g++.exe",
            r"C:\msys64\mingw64\bin\g++.exe",
            r"C:\msys64\mingw32\bin\g++.exe",
            r"C:\mingw64\bin\g++.exe",
            r"C:\mingw32\bin\g++.exe",
            r"C:\TDM-GCC-64\bin\g++.exe",
            r"C:\Program Files\mingw-w64\bin\g++.exe",
        ]
        # Also check PATH
        for p in candidates:
            if os.path.isfile(p):
                return p
        # Try PATH
        try:
            r = subprocess.run(["g++","--version"], capture_output=True, timeout=3)
            if r.returncode == 0:
                return "g++"
        except Exception:
            pass
        return None

    # ── UI construction ───────────────────────────────────────
    def _build_ui(self):
        self._build_menu()
        self._build_toolbar()
        self._build_main()
        self._build_statusbar()

    def _build_menu(self):
        mb = tk.Menu(self, bg=THEME["bg2"], fg=THEME["fg"],
                     activebackground=THEME["accent"],
                     activeforeground="white", tearoff=0)
        self.config(menu=mb)

        fm = tk.Menu(mb, bg=THEME["bg2"], fg=THEME["fg"],
                     activebackground=THEME["accent"],
                     activeforeground="white", tearoff=0)
        mb.add_cascade(label="文件", menu=fm)
        fm.add_command(label="新建        Ctrl+N", command=self._new_file)
        fm.add_command(label="打开…       Ctrl+O", command=self._open_file)
        fm.add_separator()
        fm.add_command(label="保存        Ctrl+S", command=self._save_file)
        fm.add_command(label="另存为…  Ctrl+Shift+S", command=self._save_as)
        fm.add_separator()
        fm.add_command(label="退出", command=self.quit)

        rm = tk.Menu(mb, bg=THEME["bg2"], fg=THEME["fg"],
                     activebackground=THEME["accent"],
                     activeforeground="white", tearoff=0)
        mb.add_cascade(label="运行", menu=rm)
        rm.add_command(label="▶ 运行         F5",    command=self._run)
        rm.add_command(label="⬛ 停止         F6",    command=self._stop)
        rm.add_separator()
        rm.add_command(label="查看 C++ 代码  F7",    command=self._show_cpp)
        rm.add_command(label="设置编译器路径…",      command=self._set_compiler)

        hm = tk.Menu(mb, bg=THEME["bg2"], fg=THEME["fg"],
                     activebackground=THEME["accent"],
                     activeforeground="white", tearoff=0)
        mb.add_cascade(label="帮助", menu=hm)
        hm.add_command(label="IGCSE 语法速查", command=self._show_help)
        hm.add_command(label="关于",           command=self._show_about)

    def _build_toolbar(self):
        tb = tk.Frame(self, bg=THEME["bg3"], height=44)
        tb.pack(fill=tk.X, side=tk.TOP)
        tb.pack_propagate(False)

        def btn(text, cmd, fg=THEME["fg"], bg=THEME["bg3"]):
            b = tk.Button(tb, text=text, command=cmd,
                          bg=bg, fg=fg, relief=tk.FLAT,
                          font=("Segoe UI", 10), padx=12, pady=4,
                          activebackground=THEME["border"],
                          activeforeground=THEME["fg"],
                          cursor="hand2")
            b.pack(side=tk.LEFT, padx=2, pady=6)
            return b

        btn("📄 新建", self._new_file)
        btn("📂 打开", self._open_file)
        btn("💾 保存", self._save_file)

        tk.Frame(tb, bg=THEME["border"], width=1).pack(side=tk.LEFT, fill=tk.Y, padx=6, pady=8)

        self._run_btn = btn("▶  运行", self._run,
                            fg="#ffffff", bg="#0e7a0d")
        self._run_btn.config(font=("Segoe UI", 10, "bold"))

        self._stop_btn = btn("⬛ 停止", self._stop,
                             fg="#ffffff", bg=THEME["bg3"])
        self._stop_btn.config(state=tk.DISABLED)

        btn("{ } C++ 代码", self._show_cpp)

        tk.Frame(tb, bg=THEME["border"], width=1).pack(side=tk.LEFT, fill=tk.Y, padx=6, pady=8)
        btn("? 语法帮助", self._show_help)

        # compiler indicator (right-aligned)
        self._comp_label = tk.Label(
            tb,
            text=("✓ g++ 已就绪" if self._compiler_path else "⚠ 未找到 g++，点击设置"),
            bg=THEME["bg3"],
            fg=THEME["green"] if self._compiler_path else THEME["yellow"],
            font=("Segoe UI", 9),
            cursor="hand2"
        )
        self._comp_label.pack(side=tk.RIGHT, padx=12)
        self._comp_label.bind("<Button-1>", lambda e: self._set_compiler())

    def _build_main(self):
        paned = tk.PanedWindow(self, orient=tk.VERTICAL,
                               bg=THEME["border"], sashwidth=4,
                               sashrelief=tk.FLAT)
        paned.pack(fill=tk.BOTH, expand=True)

        # ── Top: editor pane ──────────────────────────────────
        editor_frame = tk.Frame(paned, bg=THEME["bg"])
        paned.add(editor_frame, minsize=300)

        # ── Side panel: keyword reference ─────────────────────
        side = tk.Frame(editor_frame, bg=THEME["bg2"], width=200)
        side.pack(side=tk.RIGHT, fill=tk.Y)
        side.pack_propagate(False)

        tk.Label(side, text="快速参考", bg=THEME["bg2"],
                 fg=THEME["accent"], font=("Segoe UI", 10, "bold"),
                 pady=8).pack(fill=tk.X)

        ref_list = tk.Listbox(side, bg=THEME["bg2"], fg=THEME["fg"],
                              selectbackground=THEME["accent"],
                              font=("Consolas", 9), relief=tk.FLAT,
                              borderwidth=0, activestyle='none')
        ref_list.pack(fill=tk.BOTH, expand=True)
        for kw, _ in IGCSE_KEYWORDS_HELP:
            ref_list.insert(tk.END, f"  {kw}")
        ref_list.bind("<<ListboxSelect>>", self._on_ref_select)
        self._ref_list = ref_list

        self._ref_tip = tk.Label(side, text="", bg=THEME["bg3"],
                                 fg=THEME["accent2"],
                                 font=("Consolas", 8),
                                 wraplength=185, justify=tk.LEFT,
                                 padx=6, pady=6)
        self._ref_tip.pack(fill=tk.X, padx=4, pady=4)

        # ── Editor with gutter ────────────────────────────────
        edit_area = tk.Frame(editor_frame, bg=THEME["bg"])
        edit_area.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        # Gutter (line numbers)
        self._gutter = tk.Text(edit_area, width=4,
                                bg=THEME["gutter_bg"], fg=THEME["gutter_fg"],
                                font=("Consolas", 12), state=tk.DISABLED,
                                relief=tk.FLAT, padx=4,
                                selectbackground=THEME["gutter_bg"],
                                cursor="arrow", takefocus=False)
        self._gutter.pack(side=tk.LEFT, fill=tk.Y)

        # Scrollbars
        vsb = tk.Scrollbar(edit_area, orient=tk.VERTICAL,
                           bg=THEME["bg3"], troughcolor=THEME["bg3"])
        vsb.pack(side=tk.RIGHT, fill=tk.Y)
        hsb = tk.Scrollbar(edit_area, orient=tk.HORIZONTAL,
                           bg=THEME["bg3"], troughcolor=THEME["bg3"])
        hsb.pack(side=tk.BOTTOM, fill=tk.X)

        # Main text editor
        self._editor = tk.Text(
            edit_area,
            bg=THEME["bg"], fg=THEME["fg"],
            insertbackground=THEME["fg"],
            selectbackground=THEME["sel_bg"],
            font=("Consolas", 12),
            relief=tk.FLAT, padx=12, pady=8,
            undo=True, wrap=tk.NONE,
            yscrollcommand=self._on_editor_scroll,
            xscrollcommand=hsb.set,
            tabs=("2c",),
        )
        self._editor.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        vsb.config(command=self._on_vsb_scroll)
        hsb.config(command=self._editor.xview)

        self._setup_tags()

        # ── Bottom: output pane ───────────────────────────────
        out_frame = tk.Frame(paned, bg=THEME["bg2"])
        paned.add(out_frame, minsize=120)

        out_header = tk.Frame(out_frame, bg=THEME["bg3"], height=28)
        out_header.pack(fill=tk.X)
        out_header.pack_propagate(False)

        tk.Label(out_header, text="  ▸ 输出 / 终端",
                 bg=THEME["bg3"], fg=THEME["fg"],
                 font=("Segoe UI", 9, "bold")).pack(side=tk.LEFT)

        tk.Button(out_header, text="清空", command=self._clear_output,
                  bg=THEME["bg3"], fg=THEME["fg_dim"],
                  relief=tk.FLAT, font=("Segoe UI", 8),
                  activebackground=THEME["border"],
                  cursor="hand2").pack(side=tk.RIGHT, padx=8)

        # Output area with stdin support
        out_body = tk.Frame(out_frame, bg=THEME["bg2"])
        out_body.pack(fill=tk.BOTH, expand=True)

        out_vsb = tk.Scrollbar(out_body, bg=THEME["bg3"], troughcolor=THEME["bg3"])
        out_vsb.pack(side=tk.RIGHT, fill=tk.Y)

        self._output = tk.Text(
            out_body,
            bg=THEME["bg2"], fg=THEME["fg"],
            insertbackground=THEME["accent"],
            font=("Consolas", 11),
            relief=tk.FLAT, padx=8, pady=6,
            yscrollcommand=out_vsb.set,
        )
        self._output.pack(fill=tk.BOTH, expand=True)
        out_vsb.config(command=self._output.yview)

        self._output.tag_config("error",  foreground=THEME["red"])
        self._output.tag_config("warn",   foreground=THEME["yellow"])
        self._output.tag_config("system", foreground=THEME["fg_dim"])
        self._output.tag_config("ok",     foreground=THEME["green"])
        self._output.tag_config("input_prompt", foreground=THEME["accent"])

        self._output.bind("<Return>", self._on_output_enter)

    def _setup_tags(self):
        e = self._editor
        e.tag_config("keyword",  foreground=THEME["keyword_col"])
        e.tag_config("type",     foreground=THEME["type_col"])
        e.tag_config("builtin",  foreground=THEME["builtin_col"])
        e.tag_config("string",   foreground=THEME["string_col"])
        e.tag_config("number",   foreground=THEME["number_col"])
        e.tag_config("comment",  foreground=THEME["comment_col"])
        e.tag_config("op",       foreground=THEME["accent"])
        e.tag_config("ident",    foreground=THEME["fg"])
        e.tag_config("error_line", background=THEME["error_bg"])
        e.tag_config("cur_line",   background=THEME["line_bg"])

    def _build_statusbar(self):
        sb = tk.Frame(self, bg=THEME["status_bg"], height=22)
        sb.pack(fill=tk.X, side=tk.BOTTOM)
        sb.pack_propagate(False)

        self._status_left = tk.Label(
            sb, text="  IGCSE Pseudocode IDE  —  就绪",
            bg=THEME["status_bg"], fg="white",
            font=("Segoe UI", 9), anchor="w")
        self._status_left.pack(side=tk.LEFT, fill=tk.X, expand=True)

        self._status_right = tk.Label(
            sb, text="行 1，列 1  ",
            bg=THEME["status_bg"], fg="white",
            font=("Segoe UI", 9))
        self._status_right.pack(side=tk.RIGHT)

    # ── Events ────────────────────────────────────────────────
    def _bind_events(self):
        self._editor.bind("<KeyRelease>",   self._on_key)
        self._editor.bind("<ButtonRelease>",self._update_caret_pos)
        self.bind("<F5>",    lambda e: self._run())
        self.bind("<F6>",    lambda e: self._stop())
        self.bind("<F7>",    lambda e: self._show_cpp())
        self.bind("<Control-n>", lambda e: self._new_file())
        self.bind("<Control-o>", lambda e: self._open_file())
        self.bind("<Control-s>", lambda e: self._save_file())
        self.bind("<Control-S>", lambda e: self._save_as())
        self._editor.bind("<Tab>", self._on_tab)

    def _on_tab(self, e):
        self._editor.insert(tk.INSERT, "    ")
        return "break"

    def _on_key(self, e=None):
        self._modified = True
        self._update_caret_pos()
        self._update_gutter()
        # Debounce highlight
        if self._hl_job: self.after_cancel(self._hl_job)
        self._hl_job = self.after(200, self._highlight_visible)

    def _on_editor_scroll(self, *args):
        self._gutter.yview_moveto(args[0])
        # scrollbar
        # we don't have a reference here easily; handled via PanedWindow

    def _on_vsb_scroll(self, *args):
        self._editor.yview(*args)
        self._gutter.yview(*args)

    def _on_ref_select(self, e):
        sel = self._ref_list.curselection()
        if not sel: return
        _, tip = IGCSE_KEYWORDS_HELP[sel[0]]
        self._ref_tip.config(text=tip)

    def _on_output_enter(self, e):
        """Send stdin to running process."""
        if not self._proc or self._proc.poll() is not None:
            return
        line = self._output.get("input_start", tk.INSERT)
        try:
            self._proc.stdin.write(line + "\n")
            self._proc.stdin.flush()
        except Exception:
            pass
        self._output.insert(tk.END, "\n")
        self._output.mark_set("input_start", tk.END)
        return "break"

    def _update_caret_pos(self, e=None):
        idx = self._editor.index(tk.INSERT)
        row, col = idx.split(".")
        self._status_right.config(text=f"行 {row}，列 {int(col)+1}  ")

    # ── Gutter ────────────────────────────────────────────────
    def _update_gutter(self):
        content = self._editor.get("1.0", tk.END)
        nlines  = content.count("\n")
        self._gutter.config(state=tk.NORMAL)
        self._gutter.delete("1.0", tk.END)
        for i in range(1, nlines + 1):
            self._gutter.insert(tk.END, f"{i:>3}\n")
        self._gutter.config(state=tk.DISABLED)

    # ── Syntax highlighting ───────────────────────────────────
    def _highlight_all(self):
        self._update_gutter()
        self._highlight_visible()

    def _highlight_visible(self):
        e = self._editor
        # Clear all tags
        for tag in ("keyword","type","builtin","string","number","comment","op","ident"):
            e.tag_remove(tag, "1.0", tk.END)

        content = e.get("1.0", tk.END)
        lines   = content.split("\n")
        for li, line in enumerate(lines, 1):
            for start, end, tag in tokenize_for_highlight(line):
                e.tag_add(tag, f"{li}.{start}", f"{li}.{end}")

        self._highlight_current_line()

    def _highlight_current_line(self):
        e = self._editor
        e.tag_remove("cur_line", "1.0", tk.END)
        row = e.index(tk.INSERT).split(".")[0]
        e.tag_add("cur_line", f"{row}.0", f"{row}.end+1c")
        e.tag_lower("cur_line")

    # ── File operations ───────────────────────────────────────
    def _insert_sample(self):
        self._editor.insert("1.0", SAMPLE_CODE)
        self._modified = False

    def _new_file(self):
        if self._modified:
            if not messagebox.askyesno("新建", "当前文件未保存，是否丢弃？"): return
        self._editor.delete("1.0", tk.END)
        self._filepath = None
        self._modified = False
        self.title("IGCSE Pseudocode IDE")
        self._update_gutter()

    def _open_file(self):
        p = filedialog.askopenfilename(
            filetypes=[("伪代码文件","*.pseudo *.txt"), ("所有文件","*.*")])
        if not p: return
        with open(p, encoding="utf-8", errors="replace") as f:
            content = f.read()
        self._editor.delete("1.0", tk.END)
        self._editor.insert("1.0", content)
        self._filepath = p
        self._modified = False
        self.title(f"IGCSE IDE — {Path(p).name}")
        self._highlight_all()

    def _save_file(self):
        if not self._filepath: return self._save_as()
        content = self._editor.get("1.0", tk.END)
        with open(self._filepath, "w", encoding="utf-8") as f:
            f.write(content)
        self._modified = False
        self._set_status(f"已保存：{self._filepath}")

    def _save_as(self):
        p = filedialog.asksaveasfilename(
            defaultextension=".pseudo",
            filetypes=[("伪代码文件","*.pseudo"), ("文本文件","*.txt")])
        if not p: return
        self._filepath = p
        self.title(f"IGCSE IDE — {Path(p).name}")
        self._save_file()

    # ── Status ────────────────────────────────────────────────
    def _set_status(self, msg, color=None):
        self._status_left.config(
            text=f"  {msg}",
            fg=color or "white")

    # ── Output helpers ────────────────────────────────────────
    def _clear_output(self):
        self._output.config(state=tk.NORMAL)
        self._output.delete("1.0", tk.END)

    def _out(self, text, tag=None):
        self._output.config(state=tk.NORMAL)
        if tag: self._output.insert(tk.END, text, tag)
        else:   self._output.insert(tk.END, text)
        self._output.see(tk.END)

    def _mark_error_line(self, lineno):
        self._editor.tag_remove("error_line", "1.0", tk.END)
        if lineno > 0:
            self._editor.tag_add("error_line", f"{lineno}.0", f"{lineno}.end+1c")
            self._editor.see(f"{lineno}.0")

    # ── Compile & Run ─────────────────────────────────────────
    def _run(self):
        if self._proc and self._proc.poll() is None:
            return  # already running

        src = self._editor.get("1.0", tk.END)
        self._clear_output()
        self._editor.tag_remove("error_line", "1.0", tk.END)
        self._out("━━━ 正在编译… ━━━\n", "system")

        # ── Step 1: Python compiler → C++ ─────────────────────
        try:
            cpp_src = PseudoCompiler(src).compile()
        except CompileError as e:
            self._out(f"\n✗ 编译错误（第 {e.line} 行，第 {e.col} 列）：{e}\n", "error")
            self._mark_error_line(e.line)
            self._set_status(f"✗ 编译失败：{e}", THEME["red"])
            return

        # ── Step 2: Check for g++ ──────────────────────────────
        if not self._compiler_path:
            self._out("\n⚠ 未找到 g++ 编译器！\n", "warn")
            self._out("请通过菜单 运行 → 设置编译器路径 指定 g++.exe\n", "warn")
            self._out("或安装 MSYS2（https://www.msys2.org/）后重启 IDE。\n\n", "warn")
            self._out("生成的 C++ 代码：\n", "system")
            self._out(cpp_src)
            return

        # ── Step 3: Write temp file & compile ─────────────────
        tmp_dir = Path(tempfile.gettempdir()) / "igcse_ide"
        tmp_dir.mkdir(exist_ok=True)
        cpp_file = tmp_dir / "program.cpp"
        exe_file = tmp_dir / "program.exe"

        with open(cpp_file, "w", encoding="utf-8") as f:
            f.write(cpp_src)

        cmd = [self._compiler_path, "-std=c++17", "-O2",
               str(cpp_file), "-o", str(exe_file)]

        try:
            result = subprocess.run(
                cmd, capture_output=True, text=True, timeout=30)
        except FileNotFoundError:
            self._out(f"\n✗ 找不到编译器：{self._compiler_path}\n", "error")
            self._compiler_path = None
            self._comp_label.config(text="⚠ 编译器丢失，点击设置",
                                     fg=THEME["yellow"])
            return
        except subprocess.TimeoutExpired:
            self._out("\n✗ 编译超时（>30秒）\n", "error")
            return

        if result.returncode != 0:
            self._out("\n✗ g++ 编译失败：\n", "error")
            self._out(result.stderr, "error")
            return

        # ── Step 4: Run the exe ────────────────────────────────
        self._out("✓ 编译成功，正在运行…\n", "ok")
        self._out("程序将在新窗口中运行\n", "system")
        self._set_status("▶ 程序运行中…", THEME["green"])

        # 直接弹出Windows命令行窗口运行程序
        try:
            # 使用 start 命令在新窗口中运行
            cmd = f'start cmd /k "{exe_file}"'
            subprocess.Popen(cmd, shell=True)
        except Exception as ex:
            self._out(f"\n✗ 运行失败：{ex}\n", "error")
            self._on_run_done()
            return

        self._run_btn.config(state=tk.NORMAL)
        self._set_status("✓ 运行完成", THEME["green"])

    def _read_output(self):
        try:
            for line in self._proc.stdout:
                self.after(0, self._out, line)
        except Exception:
            pass

    def _read_stderr(self):
        try:
            for line in self._proc.stderr:
                self.after(0, self._out, line, "error")
        except Exception:
            pass

    def _wait_proc(self):
        self._proc.wait()
        self.after(200, self._on_run_done)

    def _on_run_done(self):
        rc = self._proc.returncode if self._proc else 0
        self._out("\n" + "─"*50 + "\n", "system")
        if rc == 0:
            self._out("✓ 程序正常结束\n", "ok")
            self._set_status("✓ 运行完成", THEME["green"])
        else:
            self._out(f"✗ 程序以代码 {rc} 退出\n", "error")
            self._set_status(f"✗ 程序异常退出（{rc}）", THEME["red"])
        self._run_btn.config(state=tk.NORMAL)
        self._stop_btn.config(state=tk.DISABLED, bg=THEME["bg3"])

    def _stop(self):
        if self._proc and self._proc.poll() is None:
            self._proc.terminate()
            self._out("\n⬛ 程序已被用户终止\n", "warn")
            self._set_status("⬛ 已停止")
            self._on_run_done()

    # ── Show generated C++ ────────────────────────────────────
    def _show_cpp(self):
        src = self._editor.get("1.0", tk.END)
        try:
            cpp = PseudoCompiler(src).compile()
        except CompileError as e:
            messagebox.showerror("编译错误", f"第 {e.line} 行：{e}")
            self._mark_error_line(e.line)
            return

        win = tk.Toplevel(self)
        win.title("生成的 C++ 代码")
        win.geometry("900x650")
        win.configure(bg=THEME["bg"])

        hdr = tk.Frame(win, bg=THEME["bg3"], height=36)
        hdr.pack(fill=tk.X)
        hdr.pack_propagate(False)
        tk.Label(hdr, text="  生成的 C++ 源代码（只读）",
                 bg=THEME["bg3"], fg=THEME["accent"],
                 font=("Segoe UI", 10, "bold")).pack(side=tk.LEFT, pady=8)

        def copy_all():
            win.clipboard_clear(); win.clipboard_append(cpp)

        tk.Button(hdr, text="复制全部", command=copy_all,
                  bg=THEME["bg3"], fg=THEME["fg"],
                  relief=tk.FLAT, font=("Segoe UI", 9),
                  cursor="hand2").pack(side=tk.RIGHT, padx=8)

        vsb = tk.Scrollbar(win)
        vsb.pack(side=tk.RIGHT, fill=tk.Y)
        txt = tk.Text(win, bg=THEME["bg"], fg=THEME["fg"],
                      font=("Consolas", 11), relief=tk.FLAT,
                      padx=12, pady=8, yscrollcommand=vsb.set,
                      state=tk.NORMAL)
        txt.pack(fill=tk.BOTH, expand=True)
        vsb.config(command=txt.yview)
        txt.insert("1.0", cpp)
        txt.config(state=tk.DISABLED)

    # ── Compiler path setting ─────────────────────────────────
    def _set_compiler(self):
        p = filedialog.askopenfilename(
            title="选择 g++.exe 编译器",
            filetypes=[("可执行文件","*.exe"), ("所有文件","*.*")])
        if not p: return
        self._compiler_path = p
        self._comp_label.config(text=f"✓ {Path(p).name}",
                                 fg=THEME["green"])
        self._set_status(f"编译器已设置：{p}")

    # ── Help window ───────────────────────────────────────────
    def _show_help(self):
        win = tk.Toplevel(self)
        win.title("IGCSE 伪代码语法速查")
        win.geometry("620x600")
        win.configure(bg=THEME["bg"])

        tk.Label(win, text="IGCSE 伪代码语法速查",
                 bg=THEME["bg"], fg=THEME["accent"],
                 font=("Segoe UI", 14, "bold"), pady=12).pack()

        txt = tk.Text(win, bg=THEME["bg2"], fg=THEME["fg"],
                      font=("Consolas", 10), relief=tk.FLAT,
                      padx=12, pady=8, state=tk.NORMAL)
        txt.pack(fill=tk.BOTH, expand=True, padx=8, pady=8)

        content = """
变量与常量
──────────────────────────────────────────
DECLARE 变量名 : 类型
CONSTANT 常量名 = 值
类型: INTEGER | REAL | STRING | BOOLEAN | CHAR
     ARRAY[下界:上界] OF 类型

赋值
──────────────────────────────────────────
变量名 ← 表达式
（也可用 <-）

输入/输出
──────────────────────────────────────────
OUTPUT 表达式 [, 表达式...]
INPUT 变量名

条件
──────────────────────────────────────────
IF 条件 THEN
    语句
ELSE
    语句
ENDIF

CASE OF 变量
    值1 : 语句
    值2 : 语句
    OTHERWISE 语句
ENDCASE

循环
──────────────────────────────────────────
FOR 变量 ← 开始 TO 结束 [STEP 步长]
    语句
NEXT 变量

WHILE 条件 DO
    语句
ENDWHILE

REPEAT
    语句
UNTIL 条件

函数与过程
──────────────────────────────────────────
FUNCTION 名称(参数列表) RETURNS 类型
    ...
    RETURN 值
ENDFUNCTION

PROCEDURE 名称([BYREF/BYVALUE] 参数 : 类型)
    ...
ENDPROCEDURE

CALL 过程名(参数)

内置函数
──────────────────────────────────────────
LENGTH(s)       字符串长度
UCASE(s)        转大写
LCASE(s)        转小写
SUBSTRING(s,i,n) 子字符串（1-based）
INT(x)          取整
SQRT(x)         平方根
ABS(x)          绝对值
MOD             取余（a MOD b）
DIV             整除（a DIV b）
"""
        txt.insert("1.0", content)
        txt.config(state=tk.DISABLED)

    def _show_about(self):
        messagebox.showinfo("关于",
            "IGCSE 伪代码 IDE\n\n"
            "支持完整 IGCSE Computer Science 伪代码语法\n"
            "内置编译器（伪代码 → C++ → exe）\n\n"
            "Windows 原生运行\n"
            "需要 MSYS2/MinGW g++ 进行最终编译")


# ══════════════════════════════════════════════════════════════════════════════
#   ENTRY POINT
# ══════════════════════════════════════════════════════════════════════════════
if __name__ == "__main__":
    app = IGCSEide()
    app.mainloop()