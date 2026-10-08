RetroTV Player  (video player + phone upload over WiFi)
========================================================

INSTALL (one time, needs the computer)
1. Open RetroTV_Player.ino in Arduino IDE (keep all the files in this folder together).
2. Near the top, set SCREEN_ROTATION to the number that worked in your old sketch (1 or 3).
   If you changed anything else in the old sketch to make your screen work, change it here too.
3. Optional: change AP_PASSWORD (at least 8 characters).
4. Board: "ESP32 Dev Module". Click Upload.
   If Arduino says "Sketch too big": Tools > Partition Scheme > "Huge APP (3MB No OTA/1MB SPIFFS)".

SEND A CLIP FROM YOUR PHONE
1. Power the TV. The screen shows a WiFi name and an address for a few seconds.
2. On the phone: join the WiFi "RetroTV" (password: retrotv123).
   The phone may warn "no internet". That is expected. Stay connected.
3. Open the browser and go to   http://192.168.4.1
4. Choose a video, pick the part you want, tap "Convert and send".
   Keep the page open until it says Done. The clip starts playing on the TV.

USE YOUR HOME WIFI INSTEAD (optional)
Put a text file named wifi.txt in the top folder of the SD card:
   line 1: your WiFi name
   line 2: your WiFi password
The TV then joins your WiFi, and the page is at http://retrotv.local
(or the number address the TV shows at power-on). Your phone keeps its internet.
If the TV cannot join, it falls back to making the "RetroTV" network.
School / work WiFi that needs a username (like eduroam) will not work.

BRIGHTNESS
The page has a Brightness slider. The TV remembers it when unplugged.

GOOD TO KNOW
- Clips are saved in the /mjpeg folder, same as before. Old clips still play.
- New clips are named like  beach.f12.mjpeg . The f12 is the speed (12 frames a second).
- Up to 40 clips. Up to 30 seconds per clip. No sound.
- Sending a clip with a name that already exists replaces the old one.
