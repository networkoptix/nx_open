from io import StringIO

from vms_benchmark.box_tests import BoxTestResult


class SudoConfigured:
    def __init__(self, dev):
        self.dev = dev

    def call(self):
        stdout = StringIO()
        error_message = StringIO()
        test_message = "this is a test text"
        res = self.dev.sh(f'echo "{test_message}"', su=True, stdout=stdout, stderr=error_message)

        obtained_test_message = stdout.getvalue().strip()

        from os import environ
        if environ.get('DEBUG', '0') == '1':
            import sys
            print(f'`{res.command}`: "{obtained_test_message}"', file=sys.stderr)

        if not res or res.return_code != 0 or obtained_test_message != test_message:
            return BoxTestResult(
                success=False,
                message=f"Command `{res.command}` failed: {str(error_message.getvalue().strip())}"
            )

        return BoxTestResult(success=True)
