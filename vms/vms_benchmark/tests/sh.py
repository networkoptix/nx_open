#!/usr/bin/python3 -b

import unittest
import sys
import traceback
from io import StringIO
from os import path as osp
import logging

project_root = osp.dirname(osp.dirname(osp.realpath(__file__)))
if osp.isfile(osp.join(project_root, ".not_installed")):
    sys.path.insert(0, osp.join(project_root, "lib"))

from vms_benchmark.box_connection import BoxConnection

logging.basicConfig(filename='test.log', filemode='w', level=logging.DEBUG)

class TestStringMethods(unittest.TestCase):
    def test_connection(self):
        device = BoxConnection(
            host='vega',
            port=22
        )
        test_command = 'echo'
        test_message = 'FOOO'
        stdout = StringIO()
        stderr = StringIO()

        res = device.sh(f'{test_command} {test_message}', stdout=stdout, stderr=stderr)

        self.assertIsNone(res.message, "Command should be executed successfully: message is None")
        self.assertEqual(res.return_code, 0, "Command should be executed successfully: return code is 0")
        self.assertTrue(res, "The result of the command execution should be evaluated to True")

        self.assertEqual(
            stdout.getvalue(),
            f'{test_message}\n',
            f"Command '{test_command} <MSG>' should be executed correctly: stdout content"
        )
        self.assertEqual(
            stderr.getvalue(),
            '',
            f"Command '{test_command} <MSG>' should be executed correctly: stderr should be empty"
        )

        for buf in [stdout, stderr]:
            buf.truncate(0)
        test_command = 'blalaecho'

        res = device.sh(f'{test_command} {test_message}', stdout=stdout, stderr=stderr)

        self.assertIsNone(res.message, "Command should be executed successfully: message is None")
        self.assertEqual(res.return_code, 127, "Command should be returned with error code 127")
        self.assertFalse(res, "The result of the command execution should be evaluated to False")

        self.assertEqual(
            stdout.getvalue(),
            '',
            f"Command '{test_command} <MSG>' should be executed correctly: stdout content"
        )
        self.assertRegex(
            stderr.getvalue(),
            f'.* {test_command}.* not found',
            f"Command '{test_command} <MSG>' should be executed correctly: stderr should describe command not found"
        )

    def test_stderr_none_acceptance(self):
        device = BoxConnection(
            host='vega',
            port=22
        )
        stdout = StringIO()
        try:
            res = device.sh('echo foo', stderr=None, stdout=stdout)
        except Exception:
            _, _, trace = sys.exc_info()
            traceback.print_tb(trace, file=sys.stderr)
            self.fail(".sh should not to raise exceptions with stderr=None")
            return

        self.assertTrue(res, ".sh should be executed successfully with stderr=None")
        self.assertEqual(res.return_code, 0, ".sh should be executed successfully with stderr=None")
        self.assertEqual(
            stdout.getvalue(),
            'foo\n',
            f".sh should be executed successfully with stderr=None"
        )


if __name__ == '__main__':
    unittest.main()