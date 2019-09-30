import requests
import re
def get_variables(cloud_url):
    systemIds = {}
    #the post request gets upset about ssl if you put the s so we remove it
    if cloud_url == "https://vm201.la.hdw.mx":
        p = re.compile("https")
        cloud_url = p.sub("http", cloud_url)
    #get the system id for the system with the autotestsanchor email and add it to the dictionary
    r = requests.post("{}/cdb/system/get".format(cloud_url), auth=requests.auth.HTTPDigestAuth("noptixautoqa+autotestsanchor@gmail.com", "qweasd 123"), json={"name":"Auto Tests"})
    s = r.json()
    systemIds["AUTO TESTS SYSTEM ID"] = s["systems"][0]["id"]

    #get the system id for the system with the autotests2anchor email and add it to the dictionary
    t = requests.post("{}/cdb/system/get".format(cloud_url), auth=requests.auth.HTTPDigestAuth("noptixautoqa+autotests2anchor@gmail.com", "qweasd 123"), json={"name":"Auto Tests"})
    u = t.json()
    systemIds["AUTOTESTS OFFLINE SYSTEM ID"] = u["systems"][0]["id"]

    #return the dictionary as variables into robot
    return systemIds