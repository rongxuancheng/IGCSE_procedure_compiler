#include "Lexer.h"
#include <sstream>
#include <iostream>
#include <algorithm>

// ─── Keyword Map ──────────────────────────────────────────────────────────────
const std::unordered_map<std::string, TokenType> Lexer::keywords_ = {
    {"DECLARE",      TokenType::KW_DECLARE},
    {"CONSTANT",     TokenType::KW_CONSTANT},
    {"INTEGER",      TokenType::KW_INTEGER},
    {"REAL",         TokenType::KW_REAL},
    {"STRING",       TokenType::KW_STRING},
    {"BOOLEAN",      TokenType::KW_BOOLEAN},
    {"CHAR",         TokenType::KW_CHAR},
    {"ARRAY",        TokenType::KW_ARRAY},
    {"OF",           TokenType::KW_OF},
    {"OUTPUT",       TokenType::KW_OUTPUT},
    {"PRINT",        TokenType::KW_PRINT},
    {"INPUT",        TokenType::KW_INPUT},
    {"IF",           TokenType::KW_IF},
    {"THEN",         TokenType::KW_THEN},
    {"ELSE",         TokenType::KW_ELSE},
    {"ENDIF",        TokenType::KW_ENDIF},
    {"FOR",          TokenType::KW_FOR},
    {"TO",           TokenType::KW_TO},
    {"STEP",         TokenType::KW_STEP},
    {"NEXT",         TokenType::KW_NEXT},
    {"WHILE",        TokenType::KW_WHILE},
    {"DO",           TokenType::KW_DO},
    {"ENDWHILE",     TokenType::KW_ENDWHILE},
    {"REPEAT",       TokenType::KW_REPEAT},
    {"UNTIL",        TokenType::KW_UNTIL},
    {"CASE",         TokenType::KW_CASE},
    {"OTHERWISE",    TokenType::KW_OTHERWISE},
    {"ENDCASE",      TokenType::KW_ENDCASE},
    {"FUNCTION",     TokenType::KW_FUNCTION},
    {"PROCEDURE",    TokenType::KW_PROCEDURE},
    {"RETURNS",      TokenType::KW_RETURNS},
    {"RETURN",       TokenType::KW_RETURN},
    {"ENDFUNCTION",  TokenType::KW_ENDFUNCTION},
    {"ENDPROCEDURE", TokenType::KW_ENDPROCEDURE},
    {"CALL",         TokenType::KW_CALL},
    {"BYVALUE",      TokenType::KW_BYVALUE},
    {"BYREF",        TokenType::KW_BYREF},
    {"AND",          TokenType::KW_AND},
    {"OR",           TokenType::KW_OR},
    {"NOT",          TokenType::KW_NOT},
    {"TRUE",         TokenType::KW_TRUE},
    {"FALSE",        TokenType::KW_FALSE},
    {"DIV",          TokenType::KW_DIV},
    {"MOD",          TokenType::KW_MOD},
};

// ─── Token::typeName ─────────────────────────────────────────────────────────
std::string Token::typeName() const {
    switch (type) {
        case TokenType::INTEGER_LIT:   return "integer literal";
        case TokenType::REAL_LIT:      return "real literal";
        case TokenType::STRING_LIT:    return "string literal";
        case TokenType::BOOLEAN_LIT:   return "boolean literal";
        case TokenType::CHAR_LIT:      return "char literal";
        case TokenType::IDENTIFIER:    return "identifier '" + value + "'";
        case TokenType::ASSIGN:        return "'←'";
        case TokenType::PLUS:          return "'+'";
        case TokenType::MINUS:         return "'-'";
        case TokenType::STAR:          return "'*'";
        case TokenType::SLASH:         return "'/'";
        case TokenType::CARET:         return "'^'";
        case TokenType::EQ:            return "'='";
        case TokenType::NEQ:           return "'<>'";
        case TokenType::LT:            return "'<'";
        case TokenType::GT:            return "'>'";
        case TokenType::LE:            return "'<='";
        case TokenType::GE:            return "'>='";
        case TokenType::LPAREN:        return "'('";
        case TokenType::RPAREN:        return "')'";
        case TokenType::LBRACKET:      return "'['";
        case TokenType::RBRACKET:      return "']'";
        case TokenType::COLON:         return "':'";
        case TokenType::COMMA:         return "','";
        case TokenType::NEWLINE:       return "newline";
        case TokenType::END_OF_FILE:   return "end of file";
        default:                       return "'" + value + "'";
    }
}

// ─── DiagEngine ──────────────────────────────────────────────────────────────
void DiagEngine::add(DiagSeverity sev, int line, int col, const std::string& msg) {
    std::string srcLine;
    if (line >= 1 && line <= (int)lines_.size())
        srcLine = lines_[line - 1];

    diags_.push_back({sev, msg, line, col, filename_, srcLine});

    if (sev == DiagSeverity::ERROR) errorCount_++;
}

void DiagEngine::error(int line, int col, const std::string& msg) {
    add(DiagSeverity::ERROR, line, col, msg);
}
void DiagEngine::warning(int line, int col, const std::string& msg) {
    add(DiagSeverity::WARNING, line, col, msg);
}
void DiagEngine::note(int line, int col, const std::string& msg) {
    add(DiagSeverity::NOTE, line, col, msg);
}

void DiagEngine::printAll(bool colors) const {
    // ANSI color codes
    const char* RED    = colors ? "\033[1;31m" : "";
    const char* YELLOW = colors ? "\033[1;33m" : "";
    const char* CYAN   = colors ? "\033[1;36m" : "";
    const char* BOLD   = colors ? "\033[1m"    : "";
    const char* RESET  = colors ? "\033[0m"    : "";
    const char* DIM    = colors ? "\033[2m"    : "";
    const char* GREEN  = colors ? "\033[1;32m" : "";

    for (const auto& d : diags_) {
        // Header: file:line:col: severity: message
        const char* sevColor = (d.severity == DiagSeverity::ERROR)   ? RED :
                               (d.severity == DiagSeverity::WARNING) ? YELLOW : CYAN;
        const char* sevLabel = (d.severity == DiagSeverity::ERROR)   ? "error" :
                               (d.severity == DiagSeverity::WARNING) ? "warning" : "note";

        std::cerr << BOLD << d.sourceFile << ":" << d.line << ":" << d.col << ": "
                  << sevColor << sevLabel << ": " << RESET
                  << BOLD << d.message << RESET << "\n";

        // Source line
        if (!d.sourceLine.empty()) {
            // Line number gutter
            std::string lineStr = std::to_string(d.line);
            std::cerr << DIM << " " << lineStr << " | " << RESET << d.sourceLine << "\n";

            // Caret
            std::string pad(lineStr.size() + 1, ' ');
            std::cerr << DIM << pad << " | " << RESET;
            int caretPos = d.col - 1;
            if (caretPos < 0) caretPos = 0;
            for (int i = 0; i < caretPos && i < (int)d.sourceLine.size(); i++) {
                std::cerr << (d.sourceLine[i] == '\t' ? '\t' : ' ');
            }
            std::cerr << GREEN << "^" << RESET << "\n";
        }
    }

    // Summary
    if (errorCount_ > 0) {
        std::cerr << RED << BOLD << "\n编译失败：" << errorCount_ << " 个错误" << RESET << "\n";
    }
}

// ─── Lexer ────────────────────────────────────────────────────────────────────
Lexer::Lexer(const std::string& source, const std::string& filename, DiagEngine& diag)
    : src_(source), filename_(filename), diag_(diag) {}

char Lexer::peek(int offset) const {
    size_t idx = pos_ + offset;
    return (idx < src_.size()) ? src_[idx] : '\0';
}

char Lexer::advance() {
    char c = src_[pos_++];
    if (c == '\n') { line_++; col_ = 1; }
    else            { col_++; }
    return c;
}

Token Lexer::makeToken(TokenType t, const std::string& val) const {
    return Token{t, val, line_, col_ - (int)val.size()};
}

void Lexer::skipWhitespace() {
    while (pos_ < src_.size()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r') advance();
        else break;
    }
}

void Lexer::skipComment() {
    // skip // ... until newline
    while (pos_ < src_.size() && peek() != '\n') advance();
}

Token Lexer::readString() {
    int startLine = line_, startCol = col_;
    advance(); // consume opening "
    std::string val;
    while (pos_ < src_.size() && peek() != '"' && peek() != '\n') {
        char c = advance();
        if (c == '\\') {
            char esc = advance();
            switch(esc) {
                case 'n': val += '\n'; break;
                case 't': val += '\t'; break;
                case '"': val += '"'; break;
                case '\\': val += '\\'; break;
                default:  val += '\\'; val += esc; break;
            }
        } else {
            val += c;
        }
    }
    if (pos_ >= src_.size() || peek() == '\n') {
        diag_.error(startLine, startCol, "字符串未闭合（缺少结束引号 \"）");
    } else {
        advance(); // consume closing "
    }
    return Token{TokenType::STRING_LIT, val, startLine, startCol};
}

Token Lexer::readChar() {
    int startLine = line_, startCol = col_;
    advance(); // consume '
    char val = '\0';
    if (pos_ < src_.size() && peek() != '\'') {
        val = advance();
    }
    if (pos_ >= src_.size() || peek() != '\'') {
        diag_.error(startLine, startCol, "字符字面量未闭合");
    } else {
        advance(); // consume closing '
    }
    return Token{TokenType::CHAR_LIT, std::string(1, val), startLine, startCol};
}

Token Lexer::readNumber() {
    int startLine = line_, startCol = col_;
    std::string num;
    bool isReal = false;
    while (pos_ < src_.size() && std::isdigit(peek())) num += advance();
    if (pos_ < src_.size() && peek() == '.' && std::isdigit(peek(1))) {
        isReal = true;
        num += advance(); // .
        while (pos_ < src_.size() && std::isdigit(peek())) num += advance();
    }
    TokenType t = isReal ? TokenType::REAL_LIT : TokenType::INTEGER_LIT;
    return Token{t, num, startLine, startCol};
}

Token Lexer::readIdentOrKeyword() {
    int startLine = line_, startCol = col_;
    std::string word;
    while (pos_ < src_.size() && (std::isalnum(peek()) || peek() == '_')) {
        word += advance();
    }
    // Convert to uppercase for keyword matching (IGCSE is case-insensitive for keywords)
    std::string upper = word;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    auto it = keywords_.find(upper);
    if (it != keywords_.end()) {
        return Token{it->second, upper, startLine, startCol};
    }
    return Token{TokenType::IDENTIFIER, word, startLine, startCol};
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (pos_ < src_.size()) {
        skipWhitespace();
        if (pos_ >= src_.size()) break;

        int curLine = line_, curCol = col_;
        char c = peek();

        // Comment
        if (c == '/' && peek(1) == '/') {
            skipComment();
            continue;
        }

        // Newline
        if (c == '\n') {
            // only emit if previous token is not already a newline (collapse blank lines)
            if (tokens.empty() || tokens.back().type != TokenType::NEWLINE) {
                tokens.push_back(Token{TokenType::NEWLINE, "\\n", curLine, curCol});
            }
            advance();
            continue;
        }

        // String
        if (c == '"') { tokens.push_back(readString()); continue; }
        // Char
        if (c == '\'') { tokens.push_back(readChar()); continue; }
        // Number
        if (std::isdigit(c)) { tokens.push_back(readNumber()); continue; }
        // Identifier / keyword
        if (std::isalpha(c) || c == '_') { tokens.push_back(readIdentOrKeyword()); continue; }

        // UTF-8 left arrow ← (U+2190, encoded as E2 86 90)
        if ((unsigned char)c == 0xE2 && pos_+2 < src_.size() &&
            (unsigned char)src_[pos_+1] == 0x86 &&
            (unsigned char)src_[pos_+2] == 0x90) {
            pos_ += 3; col_ += 1;
            tokens.push_back(Token{TokenType::ASSIGN, "←", curLine, curCol});
            continue;
        }

        advance(); // consume
        switch (c) {
            case '+': tokens.push_back(Token{TokenType::PLUS,     "+", curLine, curCol}); break;
            case '-': tokens.push_back(Token{TokenType::MINUS,    "-", curLine, curCol}); break;
            case '*': tokens.push_back(Token{TokenType::STAR,     "*", curLine, curCol}); break;
            case '/': tokens.push_back(Token{TokenType::SLASH,    "/", curLine, curCol}); break;
            case '^': tokens.push_back(Token{TokenType::CARET,    "^", curLine, curCol}); break;
            case '&': tokens.push_back(Token{TokenType::AMPERSAND,"&", curLine, curCol}); break;
            case '(': tokens.push_back(Token{TokenType::LPAREN,   "(", curLine, curCol}); break;
            case ')': tokens.push_back(Token{TokenType::RPAREN,   ")", curLine, curCol}); break;
            case '[': tokens.push_back(Token{TokenType::LBRACKET, "[", curLine, curCol}); break;
            case ']': tokens.push_back(Token{TokenType::RBRACKET, "]", curLine, curCol}); break;
            case ':': tokens.push_back(Token{TokenType::COLON,    ":", curLine, curCol}); break;
            case ',': tokens.push_back(Token{TokenType::COMMA,    ",", curLine, curCol}); break;
            case '.': tokens.push_back(Token{TokenType::DOT,      ".", curLine, curCol}); break;
            case '=': tokens.push_back(Token{TokenType::EQ,       "=", curLine, curCol}); break;
            case '>':
                if (pos_ < src_.size() && peek() == '=') {
                    advance();
                    tokens.push_back(Token{TokenType::GE, ">=", curLine, curCol});
                } else {
                    tokens.push_back(Token{TokenType::GT, ">", curLine, curCol});
                }
                break;
            case '<':
                if (pos_ < src_.size() && peek() == '-') {
                    advance();
                    tokens.push_back(Token{TokenType::ASSIGN, "<-", curLine, curCol});
                } else if (pos_ < src_.size() && peek() == '=') {
                    advance();
                    tokens.push_back(Token{TokenType::LE, "<=", curLine, curCol});
                } else if (pos_ < src_.size() && peek() == '>') {
                    advance();
                    tokens.push_back(Token{TokenType::NEQ, "<>", curLine, curCol});
                } else {
                    tokens.push_back(Token{TokenType::LT, "<", curLine, curCol});
                }
                break;
            default:
                diag_.error(curLine, curCol,
                    std::string("非法字符 '") + c + "'");
                break;
        }
    }

    tokens.push_back(Token{TokenType::END_OF_FILE, "", line_, col_});
    return tokens;
}
