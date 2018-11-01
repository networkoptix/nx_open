import requests
import time
import datetime
from check_server_var import url

def ping():
    while True:
        try:
            response = requests.get(url, timeout=5)
            print response
            break
        except requests.exceptions.ConnectionError:
            print "Server not responding", datetime.datetime.now()
    return response

if __name__ == '__main__':
    ping()