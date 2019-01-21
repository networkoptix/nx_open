from os import system, path
import threading

# list of the files that can be run simultaneously
threadList = ("register-form-validation", "login-form-validation", "change-pass-form-validation", "restore-pass-form-validation-email",
              "restore-pass-form-validation-password", "share-form-validation", "activate", "footer", "login-dialog", "login-everywhere", "misc", "register")

# list of the files that need to be run serially
serialList = ("change-pass", "account", "downloads", "history", "permissions",
              "system", "system-offline", "systems-page", "restore-pass")


class robotThread(threading.Thread):

    def __init__(self, id, name, lang, loc):
        threading.Thread.__init__(self)
        self.name = name
        self.id = id
        self.lang = lang
        self.loc = loc
    # runs the thread with the id for the output file name and the name for
    # which file to run

    def run(self):
        system('robot -v BROWSER:headlesschrome -v SCREENSHOTDIRECTORY:{} --output multi{}.xml test-cases\\{}.robot'.format(path.join(self.loc, 'combined-results'), self.id, self.name))


def threadedTestRun(loc, lang='en_US'):
    for idx, thread in enumerate(threadList):
        # uses the index of the threadlist as the id and the name as the name and
        # starts the thread
        thread = robotThread(idx, thread, lang, loc)
        thread.start()

    # runs the files that need to be run serially
    system('robot -v SCREENSHOTDIRECTORY:{} -V getvars.py:{} -N serialTest -v BROWSER:headlesschrome --output multi.xml test-cases\\{}.robot test-cases\\{}.robot test-cases\\{}.robot test-cases\\{}.robot test-cases\\{}.robot test-cases\\{}.robot test-cases\\{}.robot test-cases\\{}.robot test-cases\\{}.robot'.format(path.join(loc, 'combined-results'), lang, *serialList))
    # combines the threaded and serial tests into one results file
    print "Merging all the multithreading and serial tests"
    system('rebot -N threadTest -o output.xml multi*')



if __name__ == '__main__':
    threadedTestRun()