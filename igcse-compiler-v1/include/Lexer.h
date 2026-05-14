#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>

// ─── Token Types ──────────────────────────────────────────────────────────────
enum class TokenType {
    // Literals
    INTEGER_LIT, REAL_LIT, STRING_LIT, BOOLEAN_LIT, CHAR_LIT,

    // Identifiers & Keywords
    IDENTIFIER,
    // Declarations
    KW_DECLARE, KW_CONSTANT,
    // Types
    KW_INTEGER, KW_REAL, KW_STRING, KW_BOOLEAN, KW_CHAR, KW_ARRAY, KW_OF,
    // I/O
    KW_OUTPUT, KW_PRINT, KW_INPUT,
    // Control Flow
    KW_IF, KW_THEN, KW_ELSE, KW_ENDIF,
    KW_FOR, KW_TO, KW_STEP, KW_NEXT,
    KW_WHILE, KW_DO, KW_ENDWHILE,
    KW_REPEAT, KW_UNTIL,
    KW_CASE, KW_OF2, KW_OTHERWISE, KW_ENDCASE,
    // Functions / Procedures
    KW_FUNCTION, KW_PROCEDURE, KW_RETURNS, KW_RETURN,
    KW_ENDFUNCTION, KW_ENDPROCEDURE,
    KW_CALL, KW_BYVALUE, KW_BYREF,
    // Boolean ops
    KW_AND, KW_OR, KW_NOT,
    KW_TRUE, KW_FALSE,
    // DIV / MOD
    KW_DIV, KW_MOD,

    // Operators
    ASSIGN,         // ←  or <-
    PLUS, MINUS, STAR, SLASH, CARET,
    EQ, NEQ, LT, GT, LE, GE,
    AMPERSAND,      // string concat &

    // Punctuation
    LPAREN, RPAREN,
    LBRACKET, RBRACKET,
    COLON, COMMA, DOT,

    // Special
    NEWLINE, END_OF_FILE
};

struct Token {
    TokenType   type;
    std::string value;
    int         line;
    int         col;

    std::string typeName() const;
};

// ─── Diagnostic (error/warning) ───────────────────────────────────────────────
enum class DiagSeverity { ERROR, WARNING, NOTE };

struct Diagnostic {
    DiagSeverity severity;
    std::string  message;
    int          line;
    int          col;
    std::string  sourceFile;
    std::string  sourceLine;   // the raw source line (for caret display)
};

// ─── Diagnostic Engine ────────────────────────────────────────────────────────
class DiagEngine {
public:
    explicit DiagEngine(const std::string& filename,
                        const std::vector<std::string>& lines)
        : filename_(filename), lines_(lines) {}

    void error  (int line, int col, const std::string& msg);
    void warning(int line, int col, const std::string& msg);
    void note   (int line, int col, const std::string& msg);

    bool hasErrors() const { return errorCount_ > 0; }
    int  errorCount() const { return errorCount_; }

    void printAll(bool colors = true) const;

private:
    std::string                 filename_;
    std::vector<std::string>    lines_;
    std::vector<Diagnostic>     diags_;
    int                         errorCount_ = 0;

    void add(DiagSeverity sev, int line, int col, const std::string& msg);
};

// ─── Lexer ────────────────────────────────────────────────────────────────────
class Lexer {
public:
    Lexer(const std::string& source, const std::string& filename, DiagEngine& diag);

    std::vector<Token> tokenize();

private:
    std::string   src_;
    std::string   filename_;
    DiagEngine&   diag_;
    size_t        pos_  = 0;
    int           line_ = 1;
    int           col_  = 1;

    char peek(int offset = 0) const;
    char advance();
    void skipWhitespace();
    void skipComment();

    Token makeToken(TokenType t, const std::string& val) const;
    Token readString();
    Token readChar();
    Token readNumber();
    Token readIdentOrKeyword();
    Token readAssignOrLE();  // handles ← and <-  and <=

    static const std::unordered_map<std::string, TokenType> keywords_;
};
