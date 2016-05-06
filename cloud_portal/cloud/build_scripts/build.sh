echo "Set customization (pass customization name as a single parameter for this script)"
CUSTOMIZATION=default

if  [ -n "$1" ]; then
    CUSTOMIZATION=$1
fi

cp ../../customizations/$CUSTOMIZATION/cloud_portal.yaml $CLOUD_PORTAL_CONF_DIR

pushd ../../front_end
echo "Publish front_end ... "
grunt setbranding:$CUSTOMIZATION
grunt pub
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