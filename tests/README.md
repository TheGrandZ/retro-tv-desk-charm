# Tests

Neither set needs the real hardware.

## `page/` : the phone page in a real browser

A small fake TV (`mock_tv.py`) serves the page. A headless browser picks a video, drags the slider, converts and uploads.

```bash
pip install playwright && playwright install chromium
./page/run_tests.sh
```

`land.webm` and `port.webm` are generated test patterns (one landscape, one portrait).

## `sim/` : the firmware on a PC

The real `.ino` file is compiled for a PC. The headers in `shim/` stand in for the screen, SD card, WiFi and web server. The JPEG decoder is the real library. Everything runs under AddressSanitizer, which reports memory mistakes.

```bash
./sim/run_sim.sh
```

Expected last line: `checks passed: 43   failed: 0`
