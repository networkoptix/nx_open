echo "Set customization (pass customization name as a single parameter for this script)"

if  [ -z $CLOUD_PORTAL_CONF_DIR ]; then
    echo "Set CLOUD_PORTAL_CONF_DIR"
    export CLOUD_PORTAL_CONF_DIR=$HOME/networkoptix/develop/cloud_portal/etc
fi

CUSTOMIZATION=default

if  [ -n "$1" ]; then
    CUSTOMIZATION=$1
fi

echo "Copy YAML config ... "
cp ../../customizations/$CUSTOMIZATION/cloud_portal.yaml $CLOUD_PORTAL_CONF_DIR

echo "Copy email logo ... "
cp ../../customizations/$CUSTOMIZATION/email_logo.png ../notifications/static/templates/
cp ../../customizations/$CUSTOMIZATION/front_end/styles/* ../../front_end/app/styles/custom


pushd ../../front_end
echo "Publish front_end ... "
grunt setbranding:$CUSTOMIZATION
grunt build
popd

pushd ../notifications/static/templates/src
echo "Compile templates ... "
python preprocess.py
popd

echo "Generate ts ... "
python generate_ts.py

echo "Translate ts ... "
python localize.py


echo "Done!"

# say "Cloud portal build is finished"
