import requests

def ping():
    url = "http://cloud-test.hdw.mx/api/ping"
    response = requests.get(url)
    return response