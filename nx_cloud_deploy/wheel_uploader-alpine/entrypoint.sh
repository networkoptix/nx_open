#!/bin/sh

set -e

LIBRARY_PATH=/lib:/usr/lib /bin/sh -c "pip3 wheel --no-cache-dir -r /dev/stdin -w wheelhouse"

devpi use http://la.hdw.mx:3141/root/public --set-cfg
devpi login root --password=SuGiYaOfJ0L8OREl
devpi upload --from-dir --formats=* wheelhouse

echo Package dependencies:

scanelf --needed --nobanner --recursive /usr/local/lib/python3.6/site-packages \
        | awk '{ gsub(/,/, "\nso:", $2); print "so:" $2 }' \
        | sort -u \
        | xargs -r apk info --installed \
        | sort -u
