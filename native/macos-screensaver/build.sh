#!/bin/sh
# Builds Synchromy.saver, the native macOS screen saver (universal: Apple silicon and Intel, macOS 13+).
#
#   sh build.sh             build into ./build/Synchromy.saver (ad-hoc signed)
#   sh build.sh --install   build, then install into ~/Library/Screen Savers
#
# Requires the Xcode command line tools (clang, codesign). To distribute it outside your own Mac,
# sign it with a Developer ID certificate and notarize it.
set -e
cd "$(dirname "$0")"
MOTEUR=../engine
S=build/Synchromy.saver/Contents
rm -rf build && mkdir -p "$S/MacOS" "$S/Resources"
cp Info.plist "$S/"
cp thumbnail.png "$S/Resources/thumbnail.png"; cp thumbnail@2x.png "$S/Resources/thumbnail@2x.png"
clang -bundle -fobjc-arc -fblocks -O3 -flto -std=gnu11 -arch arm64 -arch x86_64 -mmacosx-version-min=13.0 -Xarch_arm64 -mcpu=apple-m1 \
  -I "$MOTEUR" \
  -framework Cocoa -framework ScreenSaver -framework QuartzCore -framework AudioToolbox -framework CoreAudio \
  SynchromyView.m "$MOTEUR/synchromy.c" -o "$S/MacOS/Synchromy"
codesign --force -s - build/Synchromy.saver
echo "built: $(pwd)/build/Synchromy.saver"
if [ "$1" = "--install" ]; then
  rm -rf "$HOME/Library/Screen Savers/Synchromy.saver"
  mkdir -p "$HOME/Library/Screen Savers"
  cp -R build/Synchromy.saver "$HOME/Library/Screen Savers/"
  killall legacyScreenSaver 2>/dev/null || true
  echo "installed: ~/Library/Screen Savers/Synchromy.saver"
fi
