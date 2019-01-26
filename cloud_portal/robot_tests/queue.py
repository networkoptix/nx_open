from Queue import Queue
from threading import Thread
from os import system, path
from get_names import get_threaded_names

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
num_threads = 10

# actually runs the commands in the queue


def do_stuff(q):
    while True:
        system(q.get())
        q.task_done()


def threadedTestRun(loc, lang):

    # loop through the files and add a command to run each
    for idx, file in enumerate(ThreadableFileList)
        cmdList.append('robot -v BROWSER:headlesschrome -v SCREENSHOTDIRECTORY:{} -V getvars.py:{} --output {}multi{}.xml'.format(
            path.join(loc, 'combined-results'), lang, file, idx) + ' test-cases\\' + file + '.robot')

    # loop through the files and add a command to run each test case
    for idx, file in enumerate(testList):
        cmdList.append('robot -v BROWSER:headlesschrome -v SCREENSHOTDIRECTORY:{} -V getvars.py:{} -t "'.format(path.join(loc, 'combined-results'),
                                                                                                                lang) + str(file[1]) + '" --output {}multi{}.xml'.format(str(file[0]), idx) + ' test-cases\\' + str(file[0]) + '.robot')
    # loop through the serial tests and run them
    for idx, file in enumerate(serialList):
        system('robot -v BROWSER:headlesschrome -v SCREENSHOTDIRECTORY:{} -V getvars.py:{} -e Threaded -e "Threaded File" --output {}multi{}.xml'.format(
            path.join(loc, 'combined-results'), lang, str(file), idx + 200) + ' test-cases\\' + str(file) + '.robot')

    # fill the queue with all the commands
    for x in cmdList:
        q.put(x)
    # run and manage the threads
    for i in range(num_threads):
        worker = Thread(target=do_stuff, args=(q,))
        worker.setDaemon(True)
        worker.start()

    q.join()

    # get the list of all file names
    # merge outputs with the same names to single outputs per file
    # merge single file outputs to one output file
    fileList = (test[0] for test in get_threaded_names(""))
    fileList = list(set(fileList))
    for idx, file in enumerate(fileList):
        system('rebot -o threadedRun{}.xml -R {}multi*.xml'.format(idx, str(file)))
    system('rebot -o queuedRun.xml -N  {} threadedRun*.xml'.format(lang))


if __name__ == '__main__':
    threadedTestRun("en_US")
