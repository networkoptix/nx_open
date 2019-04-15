import datetime
import time
import functools
from Queue import Queue
from threading import Thread
from os import system, path
from get_names import get_threaded_names


ENVIRONMENT = "https://cloud-test.hdw.mx"

CMD_LIST = []
# list of files that are going to be run as a whole file, but threaded
THREADABLE_FILE_LIST = ("activate", "register-form-validation", "login-form-validation",
                        "change-pass-form-validation", "restore-pass-form-validation-email",
                        "restore-pass-form-validation-password", "share-form-validation")

TEST_LIST = get_threaded_names("Threaded")

# Collect the list of tests that need to be run serially.
# Note we do not use individual cases here and only want the files themselves.
# They will be filtered by excluding the "threaded" and "threaded file"
# tags and remove repeats
SERIAL_LIST = list(set((test[0] for test in get_threaded_names("Unthreaded"))))


q = Queue(maxsize=0)
NUM_THREADS = 12

# actually runs the commands in the queue


def do_stuff(q):
    while True:
        system(q.get())
        q.task_done()

def timer(func):
    @functools.wraps(func)
    def wrapper_timer(*args, **kwargs):
        start_time = datetime.datetime.now()
        func(*args, **kwargs)
        run_time = datetime.datetime.now() - start_time
        print run_time
    return wrapper_timer

@timer
def threaded_test_run(loc, lang):

    # loop through the files and add a command to run each
    for idx, file in enumerate(THREADABLE_FILE_LIST):
        CMD_LIST.append(
            'robot --loglevel trace -v ENV:{} -v SCREENSHOTDIRECTORY:{} -V getvars.py:{} --output outputs\\{}multi{}.xml test-cases\\{}.robot'
                .format(ENVIRONMENT, path.join(loc, 'combined-results'), lang, file, idx, file))

    # loop through the files and add a command to run each test case
    for idx, file in enumerate(TEST_LIST):
        CMD_LIST.append(
            'robot --loglevel trace -v ENV:{} -v SCREENSHOTDIRECTORY:{} -V getvars.py:{} -t "{}" --output outputs\\{}multi{}.xml  test-cases\\{}.robot"'
                .format(ENVIRONMENT, path.join(loc, 'combined-results'),
                    lang, str(file[1]), str(file[0]), idx, str(file[0])))

    # loop through the serial tests and run them
    for idx, file in enumerate(SERIAL_LIST):
        system(
            'robot --loglevel trace -v ENV:{} -v SCREENSHOTDIRECTORY:{} -V getvars.py:{} -e Threaded -e "Threaded File" --output outputs\\{}multi{}.xml  test-cases\\{}.robot'
                .format(ENVIRONMENT, path.join(loc, 'combined-results'),
                       lang, str(file), idx + 200, str(file)))

    # fill the queue with all the commands
    for cmd in CMD_LIST:
        q.put(cmd)

    # run and manage the threads
    for i in range(NUM_THREADS):
        worker = Thread(target=do_stuff, args=(q,))
        worker.setDaemon(True)
        worker.start()
        #due to a post request sent by a variable file,
        #we wait a second so we don't get bad responses from the server
        time.sleep(1)

    q.join()

    # get the list of all file names
    # merge outputs with the same names to single outputs per file
    # merge single file outputs to one output file
    file_list = (test[0] for test in get_threaded_names(""))
    file_list = list(set(file_list))
    for idx, file in enumerate(file_list):
        system('rebot -o outputs\\threadedRun{}.xml -R outputs\\{}multi*.xml'
               .format(idx, str(file)))
    system('rebot --loglevel info -o queuedRun.xml -N  {} outputs\\threadedRun*.xml'
           .format(lang))

if __name__ == '__main__':
    threaded_test_run("outputs", "en_US")
