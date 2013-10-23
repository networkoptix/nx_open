SRC=./dmg-folder
TMP=tmp
VOLUME_NAME="${product.name} ${release.version}"
DMG_FILE="${product.name} ${release.version}.dmg"

ln -s /Applications $SRC/Applications
mv $SRC/client.app "$SRC/${product.name}.app"
mv "$SRC/${product.name}.app/Contents/MacOS/client" "$SRC/${product.name}.app/Contents/MacOS/${product.name}"

mkdir -p "$SRC/${product.name}.app/Contents/Resources/mac_client/bin"
mkdir -p "$SRC/${product.name}.app/Contents/Resources/mac_client/lib"
cp -Rf ${libdir}/lib/${build.configuration}/** "$SRC/${product.name}.app/Contents/Resources/mac_client/lib"
cp -Rf ${libdir}/bin/${build.configuration}/** "$SRC/${product.name}.app/Contents/Resources/mac_client/bin"

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
