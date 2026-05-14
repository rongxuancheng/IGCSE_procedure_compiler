#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>

#include "Lexer.h"
#include "Parser.h"
#include "CodeGen.h"

namespace fs = std::filesystem;

// ─── Load source file ─────────────────────────────────────────────────────────
static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "\033[1;31merror:\033[0m 无法打开文件 '" << path << "'\n";
        std::exit(1);
    }
    return {std::istreambuf_iterator<char>(f), {}};
}

// ─── Split source into lines (for diagnostics) ────────────────────────────────
static std::vector<std::string> splitLines(const std::string& src) {
    std::vector<std::string> lines;
    std::istringstream ss(src);
    std::string line;
    while (std::getline(ss, line)) lines.push_back(line);
    return lines;
}

// ─── Usage ────────────────────────────────────────────────────────────────────
static void usage(const char* prog) {
    std::cerr <<
        "用法: " << prog << " <输入文件.pseudo> [选项]\n"
        "\n"
        "选项:\n"
        "  -o <输出>       指定输出可执行文件名（默认：a.out / a.exe）\n"
        "  --emit-cpp      输出生成的 C++ 源码到 stdout\n"
        "  --save-cpp <f>  将生成的 C++ 保存到文件 f\n"
        "  --no-run        只编译，不执行\n"
        "  --ast           打印 AST（调试用）\n"
        "  --win           交叉编译为 Windows exe（需要 mingw-w64）\n"
        "  -h / --help     显示此帮助\n"
        "\n"
        "示例:\n"
        "  " << prog << " hello.pseudo\n"
        "  " << prog << " hello.pseudo -o hello\n"
        "  " << prog << " hello.pseudo --emit-cpp\n"
        "  " << prog << " hello.pseudo --win -o hello.exe\n";
    std::exit(0);
}

int main(int argc, char* argv[]) {
    if (argc < 2) { usage(argv[0]); }

    std::string inputFile;
    std::string outputFile;
    std::string saveCppFile;
    bool emitCpp = false;
    bool noRun   = false;
    bool winTarget = false;
#ifdef _WIN32
    bool isWindows = true;
#else
    bool isWindows = false;
#endif

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") usage(argv[0]);
        else if (arg == "--emit-cpp")  emitCpp  = true;
        else if (arg == "--no-run")    noRun    = true;
        
        else if (arg == "--win")       winTarget= true;
        else if (arg == "-o" && i+1 < argc) outputFile = argv[++i];
        else if (arg == "--save-cpp" && i+1 < argc) saveCppFile = argv[++i];
        else if (arg[0] != '-') inputFile = arg;
        else {
            std::cerr << "未知选项: " << arg << "\n";
            usage(argv[0]);
        }
    }

    if (inputFile.empty()) {
        std::cerr << "\033[1;31merror:\033[0m 未指定输入文件\n";
        usage(argv[0]);
    }

    // ── 1. Read source ────────────────────────────────────────────────────────
    std::string src = readFile(inputFile);
    auto lines = splitLines(src);
    DiagEngine diag(inputFile, lines);

    std::cerr << "\033[1m[IGCSE编译器]\033[0m 正在编译: " << inputFile << "\n";

    // ── 2. Lex ────────────────────────────────────────────────────────────────
    Lexer lexer(src, inputFile, diag);
    auto tokens = lexer.tokenize();

    if (diag.hasErrors()) {
        diag.printAll();
        return 1;
    }

    // ── 3. Parse ──────────────────────────────────────────────────────────────
    Parser parser(tokens, diag);

    auto ast = parser.parseProgram();

    if (diag.hasErrors()) {
        diag.printAll();
        return 1;
    }

    // ── 4. Code generation ────────────────────────────────────────────────────
    CodeGen cg(diag);
    std::string cppSrc = cg.generate(ast.get());

    if (diag.hasErrors()) {
        diag.printAll();
        return 1;
    }

    // Print any warnings
    if (diag.errorCount() == 0) {
        // print warnings only
        diag.printAll();
    }

    // ── 5. Emit / Save C++ ────────────────────────────────────────────────────
    if (emitCpp) {
        std::cout << cppSrc;
        return 0;
    }

    // Write C++ to temp file
    std::string tmpCpp = saveCppFile.empty()
        ? (fs::temp_directory_path() / "_igcse_out.cpp").string()
        : saveCppFile;

    {
        std::ofstream f(tmpCpp);
        if (!f) {
            std::cerr << "\033[1;31merror:\033[0m 无法写入临时文件 " << tmpCpp << "\n";
            return 1;
        }
        f << cppSrc;
    }

    if (!saveCppFile.empty()) {
        std::cerr << "\033[1;32m[OK]\033[0m C++ 源码已保存到: " << saveCppFile << "\n";
    }

    if (noRun) {
        std::cerr << "\033[1;32m[OK]\033[0m 生成 C++ 完成（--no-run，跳过编译）\n";
        return 0;
    }

    // ── 6. Compile C++ ───────────────────────────────────────────────────────
    std::string compiler = winTarget ? "x86_64-w64-mingw32-g++" : "g++";
    if (outputFile.empty()) {
        outputFile = (winTarget || isWindows) ? "a.exe" : "a.out";
    }

    std::string cmd = compiler +
        " -std=c++17 -O2 -Wall"
        " \"" + tmpCpp + "\""
        " -o \"" + outputFile + "\"";

    if (winTarget) cmd += " -static";

    std::cerr << "\033[1m[编译]\033[0m " << cmd << "\n";
    int ret = std::system(cmd.c_str());

    if (ret != 0) {
        std::cerr << "\033[1;31merror:\033[0m C++ 编译失败（见上方错误）\n";
        std::cerr << "       生成的 C++ 已保存至: " << tmpCpp << "\n";
        return 1;
    }

    std::cerr << "\033[1;32m[成功]\033[0m 输出: " << outputFile << "\n";

    // Cleanup temp file (unless user asked to save it)
    if (saveCppFile.empty()) {
        fs::remove(tmpCpp);
    }

    return 0;
}
