import requests
import re
def get_variables(cloud_url):
    systemIds = {}
    if cloud_url == "https://vm201.la.hdw.mx":
        p = re.compile("https")
        cloud_url = p.sub("http", cloud_url)
    r = requests.post("{}/cdb/system/get".format(cloud_url), auth=requests.auth.HTTPDigestAuth("noptixautoqa+autotestsanchor@gmail.com", "qweasd 123"), json={"name":"Auto Tests"})
    s = r.json()
    systemIds["AUTO TESTS SYSTEM ID"] = s["systems"][0]["id"]

    t = requests.post("{}/cdb/system/get".format(cloud_url), auth=requests.auth.HTTPDigestAuth("noptixautoqa+autotests2anchor@gmail.com", "qweasd 123"), json={"name":"Auto Tests"})
    u = t.json()
    systemIds["AUTOTESTS OFFLINE SYSTEM ID"] = u["systems"][0]["id"]
    return systemIds