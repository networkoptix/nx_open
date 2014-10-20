# Create new python virtualenv
${python.virtualenv} ./${arch}/python

# Replace distutils for cx_Freeze to work
rm -rf ./${arch}/python/lib/python2.7/distutils
cp -r $(${python.executable} -c "import os, distutils; print os.path.dirname(distutils.__file__)") ./${arch}/python/lib/python2.7
