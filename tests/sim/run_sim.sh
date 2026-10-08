#!/usr/bin/env bash
# Builds the real sketch for a PC (fake hardware in shim/, real JPEGDEC library) and runs the checks.
# Needs: g++, git. Run from this folder:  ./run_sim.sh
set -e
cd "$(dirname "$0")"
[ -d JPEGDEC ] || git clone --depth 1 --branch 1.8.2 https://github.com/bitbank2/JPEGDEC.git
FLAGS="-std=c++17 -g -O1 -fsanitize=address -fno-omit-frame-pointer -I shim -I JPEGDEC/src"
mkdir -p build
g++ $FLAGS -Dmemcpy_P=memcpy -c JPEGDEC/src/JPEGDEC.cpp -o build/jpegdec.o
g++ $FLAGS -x c++ sim_main.cpp -x none build/jpegdec.o -o build/sim

total_fail=0
for mode in ap empty sta stafail; do
  sd=build/sd_$mode; rm -rf "$sd"; mkdir -p "$sd/mjpeg"; echo x > "$sd/keep.txt"
  case $mode in
    ap)          cp assets/big.f12.mjpeg assets/old.mjpeg assets/portrait.f15.mjpeg assets/tail.f15.mjpeg "$sd/mjpeg/" ;;
    sta|stafail) cp assets/tail.f15.mjpeg "$sd/mjpeg/"; printf 'HomeWiFi\nsecret123\n' > "$sd/wifi.txt" ;;
  esac
  echo "=== scenario: $mode"
  ./build/sim "$sd" assets $mode > "build/out_$mode.txt" 2>&1 || true
  grep -E "^(PASS|FAIL)" "build/out_$mode.txt" || true
  f=$(grep -c "^FAIL" "build/out_$mode.txt" || true); total_fail=$((total_fail + f))
  grep -q "AddressSanitizer" "build/out_$mode.txt" && { echo "MEMORY ERROR, see build/out_$mode.txt"; total_fail=$((total_fail + 1)); }
done
echo
echo "checks passed: $(cat build/out_*.txt | grep -c '^PASS')   failed: $total_fail"
[ "$total_fail" -eq 0 ]
