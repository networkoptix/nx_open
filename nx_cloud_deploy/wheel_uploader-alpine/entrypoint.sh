#!/bin/sh

set -e

cat > requirements.txt



LIBRARY_PATH=/lib:/usr/lib /bin/sh -c "pip3 wheel --trusted-host la.hdw.mx --index-url http://la.hdw.mx:3141/root/public/ --extra-index-url http://la.hdw.mx:3141/root/pypi/ -r requirements.txt -w wheelhouse"

devpi use http://la.hdw.mx:3141/root/public --set-cfg
devpi login root --password=SuGiYaOfJ0L8OREl
devpi upload --from-dir --formats=* wheelhouse

LIBRARY_PATH=/lib:/usr/lib /bin/sh -c "pip install --no-index -f wheelhouse -r requirements.txt"

echo Package dependencies:

scanelf --needed --nobanner --recursive /usr/local/lib/python3.6/site-packages \
        | awk '{ gsub(/,/, "\nso:", $2); print "so:" $2 }' \
        | sort -u \
        | xargs -r apk info --installed \
        | sort -u
