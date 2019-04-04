from Queue import Queue
from threading import Thread
from os import system, path
from get_names import get_threaded_names
import datetime
import time

startTime = datetime.datetime.now()
cmdList = []
# list of files that are going to be run as a whole file, but threaded
ThreadableFileList = ("activate", "register-form-validation", "login-form-validation", "change-pass-form-validation", "restore-pass-form-validation-email",
                      "restore-pass-form-validation-password", "share-form-validation")

testList = get_threaded_names("Threaded")

# Collect the list of tests that need to be run serially.
# Note we do not use individual cases here and only want the files themselves.
# They will be filtered by excluding the threaded" and "threaded file"
# tags and remove repeats
serialList = list(set((test[0] for test in get_threaded_names("Unthreaded"))))


q = Queue(maxsize=0)
num_threads = 12

# actually runs the commands in the queue


def do_stuff(q):
    while True:
        system(q.get())
        q.task_done()


def threadedTestRun(loc, lang):

    # loop through the files and add a command to run each
    for idx, file in enumerate(ThreadableFileList):
        cmdList.append('robot --loglevel trace -v BROWSER:headlesschrome -v SCREENSHOTDIRECTORY:{} -V getvars.py:{} --output outputs\\{}multi{}.xml test-cases\\{}.robot'.format(path.join(loc, 'combined-results'), lang, file, idx, file))

    # loop through the files and add a command to run each test case
    for idx, file in enumerate(testList):
        cmdList.append('robot --loglevel trace -v BROWSER:headlesschrome -v SCREENSHOTDIRECTORY:{} -V getvars.py:{} -t "{}" --output outputs\\{}multi{}.xml  test-cases\\{}.robot"'.format(path.join(loc, 'combined-results'), lang, str(file[1]), str(file[0]), idx, str(file[0])))

    # loop through the serial tests and run them
    for idx, file in enumerate(serialList):
        system('robot --loglevel trace -v BROWSER:headlesschrome -v SCREENSHOTDIRECTORY:{} -V getvars.py:{} -e Threaded -e "Threaded File" --output outputs\\{}multi{}.xml  test-cases\\{}.robot'.format(
            path.join(loc, 'combined-results'), lang, str(file), idx + 200, str(file)))

    # fill the queue with all the commands
    for x in cmdList:
        q.put(x)

    # run and manage the threads
    for i in range(num_threads):
        worker=Thread(target=do_stuff, args=(q,))
        worker.setDaemon(True)
        worker.start()
        #due to a post request sent by a variable file, we wait a second so we don't get bad responses from the server
        time.sleep(1)

    q.join()

    # get the list of all file names
    # merge outputs with the same names to single outputs per file
    # merge single file outputs to one output file
    fileList=(test[0] for test in get_threaded_names(""))
    fileList=list(set(fileList))
    for idx, file in enumerate(fileList):
        system('rebot -o outputs\\threadedRun{}.xml -R outputs\\{}multi*.xml'.format(idx, str(file)))
    system('rebot --loglevel info -o queuedRun.xml -N  {} outputs\\threadedRun*.xml'.format(lang))
    print datetime.datetime.now() - startTime

if __name__ == '__main__':
    threadedTestRun("outputs", "en_US")
