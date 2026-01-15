#!/bin/bash

# 這個腳本會自動化清理、編譯，並用不同的參數執行您的 hw2 程式
# set -e 會讓腳本在任何指令執行失敗時立即停止，以避免後續的錯誤
set -e

# 1. 清理舊的編譯檔案
echo "--- 正在清理專案 ---"
make clean

# 2. 編譯程式
echo "--- 正在編譯程式 ---"
make

# 3. 執行測試案例 1 (2-way)
echo "--- 執行測試 1 (2-way) ---"
../bin/hw2 ../testcase/public1.txt ../output/public1.2way.out 2

# 4. 執行測試案例 1 (4-way)
echo "--- 執行測試 1 (4-way) ---"
../bin/hw2 ../testcase/public1.txt ../output/public1.4way.out 4

# 5. 執行測試案例 2 (2-way)
echo "--- 執行測試 2 (2-way) ---"
../bin/hw2 ../testcase/public2.txt ../output/public2.2way.out 2

# 6. 執行測試案例 2 (4-way)
echo "--- 執行測試 2 (4-way) ---"
../bin/hw2 ../testcase/public2.txt ../output/public2.4way.out 4

echo "--- 所有指令已成功執行完畢！ ---"
