#!/usr/bin/env bash
# Sweep Innovus: core utilization & clock period, then parse metrics to CSV

set -u  # 未定義變數視為錯誤
# set -e  # 若希望任何一步失敗就中止，取消註解這行

# ======= 使用者可調區 =======
UTIL_LIST=()
UTIL=0.30
while (( $(echo "$UTIL <= 0.60 + 0.001" | bc -l) )); do
  UTIL_LIST+=($(printf "%.2f" "$UTIL"))
  UTIL=$(echo "$UTIL + 0.02" | bc -l)
done

# clock period (這裡是 1850~2000 每 10)
PERIOD_LIST=()
for ((PERIOD=1850; PERIOD<=2000; PERIOD+=10)); do
  PERIOD_LIST+=("$PERIOD")
done

# Innovus 指令
INNOVUS_CMD="innovus -init apr.tcl -no_gui"

# 報告檔名（Innovus 產出）
DRC_RPT="drc.rpt"
TIMING_RPT="timing.rpt"
SUMMARY_RPT="summary.rpt"

# 產出資料夾與結果 CSV
OUT_DIR="sweep_results"
CSV_FILE="${OUT_DIR}/results.csv"

APR_TCL="apr.tcl"
SDC_FILE="../sdc/misty.sdc"

# ======= 準備環境 =======
mkdir -p "$OUT_DIR"

# 備份（只做一次）
APR_BAK="${APR_TCL}.bak_sweep"
SDC_BAK="${SDC_FILE}.bak_sweep"
if [[ ! -f "$APR_BAK" ]]; then
  cp "$APR_TCL" "$APR_BAK"
fi
if [[ ! -f "$SDC_BAK" ]]; then
  cp "$SDC_FILE" "$SDC_BAK"
fi

# 建立 CSV 標頭
if [[ ! -f "$CSV_FILE" ]]; then
  echo "core_util,clock_period,drc_violations,slack,chip_area_um2,wire_length_um" > "$CSV_FILE"
fi

# ======= 小工具：安全取值 =======
extract_drc() { awk 'match($0,/Verification[[:space:]]+Complete[[:space:]]*:[[:space:]]*([0-9]+)/,a){print a[1]}' "$1" | head -n1; }
extract_slack() { awk 'match($0,/=\s*Slack\s+Time\s+([-+]?[0-9]*\.?[0-9]+)/,a){print a[1]}' "$1" | head -n1; }
extract_area() { awk 'match($0,/Total\s+area\s+of\s+Chip:\s*([0-9]*\.?[0-9]+)/,a){print a[1]}' "$1" | head -n1; }
extract_wirelen() { awk 'match($0,/Total\s+wire\s+length:\s*([0-9]*\.?[0-9]+)/,a){print a[1]}' "$1" | head -n1; }

# ======= 主流程 =======
for UTIL in "${UTIL_LIST[@]}"; do
  for PERIOD in "${PERIOD_LIST[@]}"; do
    echo "==> Running util=${UTIL}, period=${PERIOD}"

    # 還原檔案到乾淨狀態
    cp "$APR_BAK" "$APR_TCL"
    cp "$SDC_BAK" "$SDC_FILE"

    # 1) 修改 apr.tcl 的 core utilization（只改 floorPlan 行）
    sed -E -i \
      '/^[[:space:]]*floorPlan/ s/( -r[[:space:]]*1\.0[[:space:]]*)[0-9]*\.?[0-9]+/\1'"$UTIL"'/' \
      "$APR_TCL"

    # 2) 修改 misty.sdc 的 clock period（只改 clk 那行）
    sed -E -i \
      's/(^[[:space:]]*create_clock[[:space:]]+-name[[:space:]]*"clk"[[:space:]]+-period[[:space:]]*)[0-9]*\.?[0-9]+/\1'"$PERIOD"'/g' \
      "$SDC_FILE"

    # 3) 跑 Innovus（先建立這組參數的子資料夾，再寫 log）
    RUN_TAG="util_${UTIL}_period_${PERIOD}"
    RUN_DIR="${OUT_DIR}/${RUN_TAG}"
    mkdir -p "$RUN_DIR"

    LOG_FILE="${RUN_DIR}/innovus_${RUN_TAG}.log"
    echo "    > $INNOVUS_CMD"
    $INNOVUS_CMD > "$LOG_FILE" 2>&1

    # 4) 解析報告
    DRC_VAL="NA"; SLACK_VAL="NA"; AREA_VAL="NA"; WIRE_VAL="NA"

    if [[ -f "$DRC_RPT" ]]; then
      DRC_VAL="$(extract_drc "$DRC_RPT")"; [[ -z "$DRC_VAL" ]] && DRC_VAL="NA"
      cp "$DRC_RPT" "${RUN_DIR}/drc_${RUN_TAG}.rpt"
    else
      echo "    ! Warning: $DRC_RPT not found"
    fi

    if [[ -f "$TIMING_RPT" ]]; then
      SLACK_VAL="$(extract_slack "$TIMING_RPT")"; [[ -z "$SLACK_VAL" ]] && SLACK_VAL="NA"
      cp "$TIMING_RPT" "${RUN_DIR}/timing_${RUN_TAG}.rpt"
    else
      echo "    ! Warning: $TIMING_RPT not found"
    fi

    if [[ -f "$SUMMARY_RPT" ]]; then
      AREA_VAL="$(extract_area "$SUMMARY_RPT")"; [[ -z "$AREA_VAL" ]] && AREA_VAL="NA"
      WIRE_VAL="$(extract_wirelen "$SUMMARY_RPT")"; [[ -z "$WIRE_VAL" ]] && WIRE_VAL="NA"
      cp "$SUMMARY_RPT" "${RUN_DIR}/summary_${RUN_TAG}.rpt"
    else
      echo "    ! Warning: $SUMMARY_RPT not found"
    fi

    # 寫入 CSV
    echo "${UTIL},${PERIOD},${DRC_VAL},${SLACK_VAL},${AREA_VAL},${WIRE_VAL}" >> "$CSV_FILE"
    echo "    => util=${UTIL}, period=${PERIOD} | DRC=${DRC_VAL}, Slack=${SLACK_VAL}, Area=${AREA_VAL}, WireLen=${WIRE_VAL}"
  done
done

# 跑完恢復原檔
cp "$APR_BAK" "$APR_TCL"
cp "$SDC_BAK" "$SDC_FILE"

echo
echo "All done. Results at: $CSV_FILE"
