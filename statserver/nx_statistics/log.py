#!/usr/bin/python
'''nx_statistics.log -- Default log configuration
'''

import logging

FORMAT = '[%(asctime)s] {%(pathname)s:%(lineno)d} %(levelname)s - %(message)s'
formater = logging.Formatter(FORMAT)

handler = logging.StreamHandler()
handler.setLevel(logging.INFO)
handler.setFormatter(formatter)
