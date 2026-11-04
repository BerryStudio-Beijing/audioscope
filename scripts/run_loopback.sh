#!/bin/sh
# AudioScope 1.1 — 可重复的闭环测试自检脚本（不需要音频硬件）。
#
# 1) 编译；2) 跑 DSP 自检与单元测试；3) 用“已知传递函数的虚拟 DUT”
# 跑完整分析链，并把同一设置跑两遍，逐字节比对报告，验证可重复性。
#
# 用法：  ./scripts/run_loopback.sh [输出目录]
set -e
cd "$(dirname "$0")/.."
OUT=${1:-build/repro}

echo "== 1/4 编译 =="
make -s all

echo "== 2/4 DSP 自检 + 单元测试 =="
./build/AudioScope --selftest  | tail -2
./build/AudioScope --unittest  | tail -2

echo "== 3/4 虚拟闭环（第一遍） =="
rm -rf "$OUT/a" "$OUT/b"
./build/AudioScope --simulate "$OUT/a" | tail -5

echo "== 4/4 虚拟闭环（第二遍）+ 逐字节比对 =="
./build/AudioScope --simulate "$OUT/b" > /dev/null

fail=0
for f in report.json fr.csv; do
  if cmp -s "$OUT/a/$f" "$OUT/b/$f"; then
    echo "  一致  $f"
  else
    echo "  不同  $f  <-- 不可重复"
    fail=1
  fi
done

# 报告里必须能读到关键字段
for key in '"schema": "audioscope-report/1.1"' '"verdict"' \
           '"frequency_response"' '"advice"'; do
  if ! grep -q "$key" "$OUT/a/report.json"; then
    echo "  缺少字段 $key"
    fail=1
  fi
done

if [ "$fail" -ne 0 ]; then
  echo "FAILED"
  exit 1
fi
echo "PASSED — 报告在 $OUT/a/report.json"
