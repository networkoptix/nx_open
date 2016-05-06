echo "Set localization ... "
echo "Skipped!"

echo "Publish front_end ... "
cd ../../front_end
grunt pub

echo "Compile templates ... "
cd ../cloud/notifications/static/templates/src
python preprocess.py

echo "Generate ts ... "
cd ../../../../build_scripts
python generate_ts.py

echo "Translate ts ... "
# python localize.py
echo "Skipped!"

echo "Done!"