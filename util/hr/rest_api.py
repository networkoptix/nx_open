import requests
import time
from multiprocessing import Process

def send_request():
    try:
        r = requests.get('http://demo.networkoptix.com:7001/ec2/getMediaServers', auth=('api-test', 'nxwitnessapi'), timeout=2)
        print ("Success" if r.status_code == 200 else "Error")
    except:
        print "Timeout"

def succ(num):
    for i in xrange(num):
        send_request()

def parr(num):
    threads = []
    for i in xrange(num):
        thread = Process(target=send_request)
        thread.start()
        threads.append(thread)
        
    print "All threads started"
    for thread in threads:
        thread.join()

def main():
    start = time.clock()
    parr(4)
    spent = time.clock() - start
    print "parr:"
    print spent

    start = time.clock()
    succ(4)
    spent = time.clock() - start    
    print "Succ:"
    print spent
    

if __name__ == '__main__':
    main()
