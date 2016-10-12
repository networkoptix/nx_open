# -*- coding: utf-8 -*-
# Some build dependent variables, usable for auto.py and other python scripts 
import os,sys
import requests
CUSTOMIZATION="${customization}"
CLOUD_GROUP="${cloud.group}"

if __name__ == '__main__':
    url = 'http://la.hdw.mx:8005/api/v1/cloudhostfinder/?group=%s&vms_customization=%s' % (CLOUD_GROUP, CUSTOMIZATION)
    print url
    res = requests.get(url)
    CLOUD_INSTANCE=res.text
    if ((CLOUD_INSTANCE == "Can't find matching instance") or (CLOUD_INSTANCE == "Can't find customization")):
        print >> sys.stderr, "ERROR: %s" % CLOUD_INSTANCE
        exit(1)
    else:
        f = open('cloud_instance.properties', 'w')
        print >> f, "cloudHost=%s" % CLOUD_INSTANCE
        f.close()