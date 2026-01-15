#!/bin/bash

# 設置：當任何指令失敗時，立即停止腳本
set -e

echo "===== [1/4] 清除舊的執行檔 (make clean) ====="
make clean

echo "===== [2/4] 編譯程式碼 (make) ====="
make

echo "===== [3/4] 確保輸出資料夾存在 ====="
# 根據你的路徑 ../output/output.def
# 建立 ../output 資料夾 (如果它還不存在)
mkdir -p ../output

echo "===== [4/4] 執行 public4 測試案例 ====="
# 執行 hw3
# (我修正了你指令中的 './../bin/hw3' 為 '../bin/hw3')
../bin/hw3 ../testcase/public4.lef ../testcase/public4.def ../output/output.def

echo "===== 執行完畢 ====="

