import requests
import time
import datetime

def ping():
    url = "http://cloud-test.hdw.mx/api/ping"
    while True:
        try:
            response = requests.get(url, timeout=5)
            break
        except requests.exceptions.ConnectionError:
            print "Server not responding", datetime.datetime.now()
    return response