BINARIES=${libdir}/bin/${build.configuration}
LIBRARIES=${libdir}/lib/${build.configuration}
SRC=./dmg-folder
TMP=tmp
VOLUME_NAME="${product.name} ${release.version}"
DMG_FILE="${product.name} ${release.version}.dmg"
HELP=${libdir}/help

ln -s /Applications $SRC/Applications
mv $SRC/client.app "$SRC/${product.name}.app"
mv "$SRC/DS_Store" "$SRC/.DS_Store"
python macdeployqt.py "$SRC/${product.name}.app" "$BINARIES" "$LIBRARIES" "$HELP"
codesign -f -v --deep -s "${mac.sign.identity}" "$SRC/${product.name}.app"

SetFile -c icnC $SRC/.VolumeIcon.icns
hdiutil create -srcfolder $SRC -volname "$VOLUME_NAME" -format UDRW -ov "raw-$DMG_FILE"

[ "$1" == "rwonly" ] && exit 0

rm -rf $TMP
mkdir -p $TMP

hdiutil attach "raw-$DMG_FILE" -mountpoint "$TMP"
SetFile -a C "$TMP"
hdiutil detach "$TMP"
rm -rf "$TMP"
rm -f "$DMG_FILE"
hdiutil convert "raw-$DMG_FILE" -format UDZO -o "$DMG_FILE"
rm -f "raw-$DMG_FILE"
