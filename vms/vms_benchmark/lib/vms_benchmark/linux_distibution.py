from .exceptions import VmsBenchmarkError
from io import StringIO


class LinuxDistributionDetector:
    class LinuxDistribution:
        def __init__(self, name, family_name, version, kernel_version, with_systemd):
            self.name = str(name)
            self.family_name = str(family_name)
            self.version = str(version)
            self.kernel_version = list(int(version_component) for version_component in kernel_version)
            self.with_systemd = with_systemd

    class LinuxDistributionDetectionError(VmsBenchmarkError):
        pass

    @staticmethod
    def detect(device):
        command = r"uname -r | grep -Eo '[0-9]+\.[0-9]+(\.[0-9]+)?'"
        kernel_version = device.eval(command).split('.')

        if not kernel_version:
            raise LinuxDistributionDetector.LinuxDistributionDetectionError(
                message="Can't obtain kernel version.\n" +
                        f"Command `{command}` failed."
            )

        os_name = None
        os_family_name = None
        os_version = None
        with_systemd = False

        res = device.get_file_content('/etc/os-release', su=True, stderr=None)

        if res:
            def unquote_value_if_needed(key, value):
                import re
                return key, re.sub(r"^(\"(.*)\"|(.*))$", r'\g<2>\g<3>', value)

            info = dict(unquote_value_if_needed(*line.split('=')) for line in res.split('\n') if len(line.strip()) > 0)

            if info.get('ID', None) == 'debian':
                os_name = 'debian'
                os_family_name = 'debian'
                os_version = info.get('VERSION_ID')
                if int(os_version) >= 8:
                    with_systemd = True
            elif info.get('ID', None) == 'ubuntu':
                os_name = 'ubuntu'
                os_family_name = 'debian'
                os_version = info.get('VERSION_ID')

                if int(os_version.replace('.', '')) >= 1604:
                    with_systemd = True

        if not os_name:
            os_name = 'custom'
            os_family_name = 'none'
            os_version = 'none'

        return LinuxDistributionDetector.LinuxDistribution(
            name=os_name,
            family_name=os_family_name,
            version=os_version,
            kernel_version=kernel_version,
            with_systemd=with_systemd
        )
