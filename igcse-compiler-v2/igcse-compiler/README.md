# IGCSE Pseudocode Compiler

将 IGCSE Computer Science 伪代码编译为可执行程序。

## 支持的语法

- 变量声明: `DECLARE x : INTEGER`
- 赋值: `x ← 10`
- 输出: `OUTPUT "Hello"` / `PRINT x`
- 输入: `INPUT x`
- 条件: `IF ... THEN ... ELSE ... ENDIF`
- 循环: `FOR`, `WHILE`, `REPEAT ... UNTIL`
- 数组: `DECLARE arr : ARRAY[1:10] OF INTEGER`
- 函数/过程: `FUNCTION`, `PROCEDURE`
- 注释: `//`

## 构建方式

### 在 WSL 中构建 Linux 版本
```bash
mkdir build && cd build
cmake ..
make
```

### 在 WSL 中交叉编译 Windows exe
```bash
sudo apt install mingw-w64
mkdir build-win && cd build-win
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/win64-toolchain.cmake
make
```

## 使用方法
```
igcse-compiler input.pseudo              # 编译并运行
igcse-compiler input.pseudo -o output   # 编译输出可执行文件
igcse-compiler input.pseudo --emit-cpp  # 输出生成的 C++ 代码
igcse-compiler input.pseudo --ast       # 显示 AST
```
