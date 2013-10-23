BINARIES=${libdir}/bin/${build.configuration}
LIBRARIES=${libdir}/lib/${build.configuration}
SRC=./dmg-folder
TARGET="$SRC/${product.name}.app/Contents/Resources/mac_client"
TMP=tmp
VOLUME_NAME="${product.name} ${release.version}"
DMG_FILE="${product.name} ${release.version}.dmg"

ln -s /Applications $SRC/Applications
mv $SRC/client.app "$SRC/${product.name}.app"
mv "$SRC/DS_Store" "$SRC/.DS_Store"
mv "$SRC/${product.name}.app/Contents/MacOS/client" "$SRC/${product.name}.app/Contents/MacOS/${product.name}"
chmod 777 "$SRC/${product.name}.app/Contents/MacOS/${product.name}"
chmod 777 "$SRC/${product.name}.app/Contents/Resources/script"

mkdir -p "$TARGET/bin"
mkdir -p "$TARGET/lib"
cp -Rf $LIBRARIES/** "$TARGET/lib"
cp -Rf $BINARIES/** "$TARGET/bin"
find "$TARGET/bin" -type f -maxdepth 1 -exec rm -f {} \;
find "$TARGET/bin" -lname '*.*' -maxdepth 1 -exec rm -f {} \;
cp -f $BINARIES/client "$TARGET/bin"
cp -f $BINARIES/applauncher "$TARGET/bin"
cp -f $BINARIES/x264 "$TARGET/bin"

SetFile -c icnC $SRC/.VolumeIcon.icns
hdiutil create -srcfolder $SRC -volname "$VOLUME_NAME" -format UDRW -ov "raw-$DMG_FILE"

# exit 0
rm -rf $TMP
mkdir -p $TMP

hdiutil attach "raw-$DMG_FILE" -mountpoint "$TMP"
SetFile -a C "$TMP"
hdiutil detach "$TMP"
rm -rf "$TMP"
rm -f "$DMG_FILE"
hdiutil convert "raw-$DMG_FILE" -format UDZO -o "$DMG_FILE"
rm -f "raw-$DMG_FILE"
