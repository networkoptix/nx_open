from _pytest.assertion.util import assertrepr_compare


class CompareConfig(object):

    def __init__(self, verbose):
        self._verbose = verbose

    def getoption(self, name):
        assert name == 'verbose', repr(name)  # No other options are supported
        return self._verbose


# returns line list
def compare_values(x, y, verbose=True):
    return assertrepr_compare(CompareConfig(verbose), '==', x, y)
