import hashlib
import random
import string
import os

TABLE = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'

serials = {}

def gen_serial():
    return string.join([TABLE[random.randrange(0, len(TABLE))] for x in range(10)], '')

for n in xrange(1,1001):
    serial = gen_serial()

    # serails should be unique
    while serial in serials:
      serial = gen_serial()

    serials[serial] = hashlib.md5(serial).hexdigest()

serials_txt = open('serials.txt', 'w')
serials_ipp = open('serials.ipp', 'w')
for n, (serial, xhash) in enumerate(serials.items()):
    f = open(os.path.join('serials', '%.3d.txt' % n), 'w')
    print >> f, serial
    f.close()

    print >> serials_ipp, 'm_serialHashes.push_back(QLatin1String("%s"));' % xhash
    print >> serials_txt, serial

serials_txt.close()
serials_ipp.close()
