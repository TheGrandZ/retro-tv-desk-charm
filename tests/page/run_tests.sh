#!/usr/bin/env bash
# Tests the phone page in a headless browser against a fake TV (no hardware needed).
# Needs: python3, pip install playwright, then: playwright install chromium
set -e
cd "$(dirname "$0")"
rm -rf store
python3 mock_tv.py ../../firmware/page store &
MOCK=$!
trap 'kill $MOCK 2>/dev/null' EXIT
sleep 1
python3 run_page_test.py
python3 run_brightness_test.py
