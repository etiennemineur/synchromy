#!/bin/sh
# Builds Synchromy for Windows (x64) with mingw-w64: Synchromy.exe (the app) and Synchromy.scr (the screen saver),
# then packs them into build/Synchromy-Windows.zip. The engine is the one shared with the macOS screen saver and the Apple TV app.
#
#   macOS:  brew install mingw-w64
#   Linux:  apt install gcc-mingw-w64-x86-64 zip
set -e
cd "$(dirname "$0")"
MOTEUR=../engine
rm -rf build && mkdir -p build/Synchromy
x86_64-w64-mingw32-windres synchromy.rc -O coff -o build/resources.o
x86_64-w64-mingw32-gcc -O3 -std=gnu11 -mwindows -static -I "$MOTEUR" -o build/Synchromy/Synchromy.exe synchromy-win.c "$MOTEUR/synchromy.c" build/resources.o \
  -lole32 -lcomctl32 -ldwmapi -lwinmm -luuid -lpthread
cp build/Synchromy/Synchromy.exe build/Synchromy/Synchromy.scr
cp README.txt LISEZ-MOI.txt build/Synchromy/
(cd build && rm -f Synchromy-Windows.zip && zip -q -r Synchromy-Windows.zip Synchromy)
rm -f build/resources.o
echo "built: $(pwd)/build/Synchromy-Windows.zip"
