from robot.api import TestSuiteBuilder, TestSuite
from os import path


def get_threaded_names(tag):

    suite = TestSuiteBuilder().build(path.join("C:\\", "develop", "nx_vms",
                                               "cloud_portal", "robot_tests", "test-cases"))
    if tag == "Threaded":
        suite.filter(included_tags=tag)
    elif tag == "Unthreaded":
        suite.filter(excluded_tags="Threaded File")
        suite.filter(excluded_tags="Threaded")

    theList = []
    for suite in suite.suites:
        for test in suite.tests:
            theList.append((suite, test))
    stuff = (item[0] for item in theList)
    stuff = list(set(stuff))

    return theList

if __name__ == '__main__':
    get_threaded_names("")
