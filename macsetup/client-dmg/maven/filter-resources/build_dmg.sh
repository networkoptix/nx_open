BINARIES=${libdir}/bin/${build.configuration}
LIBRARIES=${libdir}/lib/${build.configuration}
SRC=./dmg-folder
TMP=tmp
VOLUME_NAME="${product.name} ${release.version}"
DMG_FILE="${project.build.finalName}.dmg"
HELP=${environment.dir}/help/${release.version}/${customization}

AS_SRC=app-store
PKG_FILE="${project.build.finalName}.pkg"

ln -s /Applications $SRC/Applications

mv $SRC/client.app "$SRC/${product.name}.app"
mv "$SRC/DS_Store" "$SRC/.DS_Store"
python macdeployqt.py "$SRC/${product.name}.app" "$BINARIES" "$LIBRARIES" "$HELP"
security unlock-keychain -p 123 $HOME/Library/Keychains/login.keychain

if [ '${skip.sign}' == 'false'  ]
then
    codesign -f -v --deep -s "${mac.sign.identity}" "$SRC/${product.name}.app"
fi

# Boris, move this to a separate script (of even folder), please
rm -rf "$AS_SRC"
mkdir "$AS_SRC"
cp -a "$SRC/${product.name}.app" "$AS_SRC"

if [ '${skip.sign}' == 'false'  ]
then
    codesign -f -v --deep --entitlements entitlements.plist -s "${mac.app.sign.identity}" "$AS_SRC/${product.name}.app"
    productbuild --component "$AS_SRC/${product.name}.app" /Applications --sign "${mac.pkg.sign.identity}" --product "$AS_SRC/${product.name}.app/Contents/Info.plist" "$PKG_FILE"
    # End
fi

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
