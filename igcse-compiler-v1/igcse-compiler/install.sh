#!/bin/bash
# ══════════════════════════════════════════════════════════════
#  IGCSE 伪代码编译器  —  WSL 一键安装 & 构建脚本
#  支持：Ubuntu 20.04 / 22.04 / 24.04 on WSL2
# ══════════════════════════════════════════════════════════════
set -e

RED='\033[1;31m'; GREEN='\033[1;32m'; BLUE='\033[1;34m'; RESET='\033[0m'
info()  { echo -e "${BLUE}[INFO]${RESET} $*"; }
ok()    { echo -e "${GREEN}[OK]${RESET}   $*"; }
err()   { echo -e "${RED}[ERR]${RESET}  $*" >&2; exit 1; }

echo -e "\n${BLUE}══════════════════════════════════════════${RESET}"
echo -e "${BLUE}   IGCSE 伪代码编译器  —  WSL 安装脚本   ${RESET}"
echo -e "${BLUE}══════════════════════════════════════════${RESET}\n"

# ── 1. 安装依赖 ───────────────────────────────────────────────
info "更新软件包列表..."
sudo apt-get update -qq

info "安装 g++, cmake, make..."
sudo apt-get install -y -qq g++ cmake make

# ── 2. 可选：安装 MinGW 交叉编译器（输出 Windows .exe）───────
read -p "是否安装 mingw-w64 以支持编译 Windows exe？ [y/N] " ans
if [[ "$ans" =~ ^[Yy]$ ]]; then
    info "安装 mingw-w64..."
    sudo apt-get install -y -qq mingw-w64
    ok "已安装 mingw-w64，可用 --win 选项编译 .exe"
fi

# ── 3. 构建 ────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
info "构建目录: $SCRIPT_DIR"

mkdir -p "$SCRIPT_DIR/build"
cd "$SCRIPT_DIR/build"

info "运行 CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-w"

info "编译..."
make -j$(nproc)

ok "编译成功！可执行文件: $SCRIPT_DIR/build/igcse-compiler"

# ── 4. 可选：安装到 /usr/local/bin ────────────────────────────
read -p "是否安装到 /usr/local/bin/igcse（全局可用）？ [y/N] " ans2
if [[ "$ans2" =~ ^[Yy]$ ]]; then
    sudo cp "$SCRIPT_DIR/build/igcse-compiler" /usr/local/bin/igcse
    ok "已安装到 /usr/local/bin/igcse"
    echo ""
    echo -e "${GREEN}现在可以直接使用:${RESET}"
    echo -e "  igcse hello.pseudo"
else
    echo ""
    echo -e "${GREEN}可以这样使用:${RESET}"
    echo -e "  $SCRIPT_DIR/build/igcse-compiler hello.pseudo"
fi

# ── 5. 运行测试 ────────────────────────────────────────────────
info "运行测试用例..."
COMPILER="$SCRIPT_DIR/build/igcse-compiler"

echo ""
echo -e "${BLUE}─── 测试：函数与过程 ───────────────────────${RESET}"
echo "7" | "$COMPILER" "$SCRIPT_DIR/tests/03_functions.pseudo" --no-run 2>&1 | head -3

echo ""
echo -e "${GREEN}══════════════════════════════════════════${RESET}"
echo -e "${GREEN}   安装完成！                              ${RESET}"
echo -e "${GREEN}══════════════════════════════════════════${RESET}"
echo ""
echo "用法示例："
echo "  igcse input.pseudo              # 编译并运行"
echo "  igcse input.pseudo -o out       # 输出可执行文件"
echo "  igcse input.pseudo --emit-cpp   # 查看生成的 C++ 代码"
echo "  igcse input.pseudo --win -o a.exe  # 编译为 Windows exe"
