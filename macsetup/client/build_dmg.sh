echo "Boris, put mac_client folder to dmg-folder/DW\ Spectrum.app/Contents/Resources/"
exit 0

SRC=dmg-folder
TMP=tmp
VOLUME_NAME="DW Spectrum 2.1"
DMG_FILE="DW Spectrum 2.1.dmg"

SetFile -c icnC $SRC/.VolumeIcon.icns
hdiutil create -srcfolder $SRC -volname "$VOLUME_NAME" -format UDRW -ov "raw-$DMG_FILE"

rm -rf $TMP
mkdir -p $TMP

hdiutil attach "raw-$DMG_FILE" -mountpoint "$TMP"
SetFile -a C "$TMP"
hdiutil detach "$TMP"
rm -rf "$TMP"
rm -f "$DMG_FILE"
hdiutil convert "raw-$DMG_FILE" -format UDZO -o "$DMG_FILE"
rm -f "raw-$DMG_FILE"
