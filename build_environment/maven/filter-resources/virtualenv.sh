# Create new python virtualenv
rm -rf python
virtualenv python

# Replace distutils for cx_Freeze to work
rm -rf python/lib/python2.7/distutils
cp -r $(python -c "import os, distutils; print os.path.dirname(distutils.__file__)") python/lib/python2.7