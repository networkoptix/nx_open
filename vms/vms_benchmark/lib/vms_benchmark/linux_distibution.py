import re
from .exceptions import VmsBenchmarkError
from typing import Tuple


class LinuxDistributionDetector:
    class LinuxDistribution:
        def __init__(self, name, family_name, version, kernel_version, with_systemd):
            self.name = str(name)
            self.family_name = str(family_name)
            self.version = str(version)
            self._kernel_version = kernel_version
            self.with_systemd = with_systemd

        def is_kernel_version_detected(self) -> bool:
            return len(self._kernel_version) != 0

        def supports_mem_available(self) -> bool:
            if not self.is_kernel_version_detected():
                # If we can't determine the kernel version, assume that the system supports
                # "MemAvailable" parameter in /proc/meminfo. If we are wrong, then we log the error
                # when we try to obtain "MemAvailable" value and continue running the tests.
                return True

            if self._kernel_version[0] < 3:
                return False

            if self._kernel_version[0] == 3 and self._kernel_version[1] < 14:
                return False

            return True

        def get_kernel_version(self) -> str:
            if not self.is_kernel_version_detected():
                return 'unknown'

            return '.'.join(str(c) for c in self._kernel_version)


    class LinuxDistributionDetectionError(VmsBenchmarkError):
        pass

    @staticmethod
    def detect(device):
        command = r"uname -r"
        output = device.eval(command)
        kernel_version = LinuxDistributionDetector._parse_uname_output(output)

        with_systemd = False

        res = device.get_file_content('/etc/os-release', su=True, stderr=None)

        if res:
            def unquote_value_if_needed(key, value):
                return key, re.sub(r"^(\"(.*)\"|(.*))$", r'\g<2>\g<3>', value)

            info = dict(unquote_value_if_needed(*line.split('=')) for line in res.split('\n') if len(line.strip()) > 0)

            os_name = info.get('ID')
            os_family_name = info.get('ID_LIKE')
            os_version = info.get('VERSION_ID')
        else:
            os_name = 'custom'
            os_family_name = 'none'
            os_version = 'none'

        init_link = device.eval('readlink /sbin/init')

        if isinstance(init_link, str) and init_link.split('/')[-1] == 'systemd':
            with_systemd = True

        return LinuxDistributionDetector.LinuxDistribution(
            name=os_name,
            family_name=os_family_name,
            version=os_version,
            kernel_version=kernel_version,
            with_systemd=with_systemd
        )

    @staticmethod
    def _parse_uname_output(output: str) -> Tuple[int, ...]:
        # The output of `uname` command should be exactly one line, containing the kernel version
        # in the folowing format:
        # ```
        # <major_number>.<minor_number>[.<revision_number>][<other_version_info>]
        # ```
        match = re.match(r"^(?P<major>\d+)\.(?P<minor>\d+)(?:\.(?P<revision>\d+))?", output)
        if match is None:
            return tuple()

        if match['revision'] is None:
            return int(match['major']), int(match['minor'])

        return int(match['major']), int(match['minor']), int(match['revision'])
