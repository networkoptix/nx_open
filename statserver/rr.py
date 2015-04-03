#!/usr/bin/python
'''Creates and reports randome data
'''

import json
import requests
import random
import uuid

def post(content):
    r = requests.post('http://localhost/api/reportStatistics',
                      data=json.dumps(content))
    return r.text

def rint(n): return random.randint(int(n/2), n)
def uuids(n=1000): return ("{%s}" % uuid.uuid4() for i in range(n))

def rval(prefix, n):
    while 1:
        yield '%s %s' % (prefix, random.randint(1, n))

def rset(*vals):
    pool = list(vals)
    while 1:
        yield random.choice(pool)

def rows(n, **vals):
    rs = list([dict() for i in range(n)])
    for name, gen in vals.items():
        it = iter(gen)
        for r in rs: r[name] = it.next()
    return rs

def data():
    random.seed()
    MSC = rint(5)
    MSS = list(uuids(MSC))

    return dict(
        cameras = rows(rint(20),
            id = uuids(),
            parentId = rset(*MSS),
            model = rval('Type %i', 5),
            vendor = rset('CoolCams', 'UsualCams'),
        ),
        mediaservers = rows(MSC,
            id = MSS,
            cpuArchitecture = rset('x86', 'x86_64'),
            cpuModelName = rset('Intel XYZ', 'Celeron ABC'),
        ),
        systemId = iter(uuids(1)).next(),
    )

if __name__ == '__main__':
    print post(data())

