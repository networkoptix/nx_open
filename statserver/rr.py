#!/usr/bin/python
'''Creates and reports randome data
'''

import argparse, math
import json, requests
import random, uuid

def post(content):
    r = requests.post('http://localhost:8000/api/reportStatistics',
                      data=json.dumps(content))
    return r.text

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

def data(extra=1):
    random.seed()
    def ceil(n): return int(math.ceil(n))
    def rint(n): return random.randint(ceil(n * extra / 2), ceil(n * extra))
    def uuids(n=1000): return ("{%s}" % uuid.uuid4() for i in range(n))

    MSC = rint(5)
    MSS = list(uuids(MSC))
    return dict(
        businessRules = rows(rint(150),
            id = uuids(),
            actionType = rval('', 5),
            aggregationPeriod = rval('', 30000),
            disabled = rset(True, False),
            eventState = rval('', 3),
            eventType = rval('', 4),
        ),
        cameras = rows(rint(100),
            id = uuids(),
            parentId = rset(*MSS),
            model = rval('Type %i', 5),
            vendor = rset('CoolCams', 'UsualCams'),
            audioEnabled = rset(True, False),
            controlEnabled = rset(True, False),
            manuallyAdded = rset(True, False),
            status = rset('Online', 'Offline'),
        ),
        clients = rows(rint(20),
            id = uuids(),
            parentId = rset(*MSS),
            cpuArchitecture = rset('x86', 'x86_64'),
            cpuModelName = rval('Intel MX', 3),
            openGLRenderer = rval("GeForce XZ", 10),
            openGLVendor = rset("NVIDIA Corporation"),
            openGLVersion = rval("4.4.", 5),
            phisicalMemory = rval('', 10000),
        ),
        licenses = rows(rint(3),
            brand = rset('hdwitness', 'elseone'),
            cameraCount = rval('', 100),
            licenseType = rset('degital', 'box'),
            name = rset('hdwitness'),
            version = rval('', 25),
        ),
        mediaservers = rows(MSC,
            id = MSS,
            cpuArchitecture = rset('x86', 'x86_64', 'arm'),
            cpuModelName = rval('Intel FX', 5),
            phisicalMemory = rval('', 16000),
            status = rset(True),
#            storages = rows(rint(3),
#                id = uuids(),
#                parentId = rset(*MSS),
#                spaceLimit = rval('', 10000000),
#                usedForWriting = rset(True),
#            ),
            systemInfo =  rset('linux x64 ubuntu', 'windows x64 winxp', 'windows x64 win7'),
            version = rval('2.3.', 3),
        ),
        systemId = iter(uuids(1)).next(),
    )

def main():
    p = argparse.ArgumentParser(description="Generates random reports")
    p.add_argument('-c', dest='console', action='store_true')
    p.add_argument('-x', dest='extra', type=float, default=1)

    a = p.parse_args()
    d = data(a.extra)
    if a.console:
        from pprint import pprint
        pprint(d)
    else:
        post(d)

if __name__ == '__main__':
    main()

