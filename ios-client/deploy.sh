set -e

get_var()
{
    echo $(< target/env/$1)
}

if [ "$1" = "hdw" ] 
then
    ROOT_URL=https://ios.hdw.mx
    SSH_TARGET=hdw:/srv/ios/public
else
    ROOT_URL=https://noptix.enk.me/ios
    SSH_TARGET=rsync://noptix.enk.me/ios
fi

NAME=$(get_var namespace.additional)
BUNDLE_IDENTIFIER=$(get_var ios.bundle_identifier)
PRODUCT_NAME=$(get_var product.name)
DISPLAY_PRODUCT_NAME=$(get_var display.product.name)
BUILD_NUMBER=$(get_var buildNumber)
BRANCH=$(hg branch)
NAME_BRANCH_BUILD=${NAME}-${BRANCH}-${BUILD_NUMBER}
BASE_URL=$ROOT_URL/$NAME_BRANCH_BUILD

STAGE=stage/ios/$NAME_BRANCH_BUILD
rm -rf $STAGE
mkdir -p $STAGE

cat deploy_index.html | sed "s/\${NAME}/$NAME/g
s%\${BASE_URL}%$BASE_URL%g
s/\${BRANCH}/$BRANCH/g" > $STAGE/index.html

cp target/$NAME-*-test.ipa $STAGE/$NAME.ipa
cp target/images/icon_72x72.png target/images/icon_512x512.png $STAGE
cat deploy.plist | sed "s/\${NAME}/$NAME/g
s/\${BRANCH}/$BRANCH/g
s%\${BASE_URL}%$BASE_URL%g
s%\${RANDOM}%$RANDOM%g
s%\${BUILD_NUMBER}%$BUILD_NUMBER%g
s/\${ios.bundle_identifier}/$BUNDLE_IDENTIFIER/g
s/\${display.product.name}/$DISPLAY_PRODUCT_NAME/g
" > $STAGE/$NAME.plist

rsync -a --delete $STAGE $SSH_TARGET
# python rsputfolder.py stage

# qrcode-terminal $BASE_URL
echo "Deployed. Use $BASE_URL"
