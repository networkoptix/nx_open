import logging
import tempfile
from pprint import pprint, pformat

from vms_benchmark import exceptions
from vms_benchmark.config import ConfigParser
from vms_benchmark.box_platform import BoxPlatform
from vms_benchmark.exceptions import BoxCommandError, BoxStateError, HostOperationError, BoxFileContentError


class VmsScanner:
    class Vms:
        def __init__(self, device, linux_distribution, pid, customization, dir, host, port):
            self.customization = customization
            self.dir = dir
            self.host = host
            self.port = port
            self.device = device
            self.linux_distribution = linux_distribution
            self.pid = pid

        def is_up(self):
            pids_raw = self.device.eval(f'pidof {self.server_bin(self.linux_distribution)}', stderr=None)
            if not pids_raw:
                return False

            return self.pid in [int(pid) for pid in pids_raw.strip().split()]

        def uid(self):
            res = self.device.eval(fr"cat /proc/{self.pid}/status | grep -o 'Uid:\s\+[0-9]\+' | grep -o '[0-9]\+'")
            if res:
                try:
                    return int(res)
                except ValueError:
                    return None
            return None

        def execute_service_command(self, command, exc=False):
            try:
                if self.linux_distribution.with_systemd:
                    self.device.sh(
                        f'systemctl {command} {self.customization}-mediaserver',
                        timeout=30,
                        su=True,
                        exc=True,
                        stderr=None,
                        stdout=None
                    )
                else:
                    self.device.sh(
                        f'/etc/init.d/{self.customization}-mediaserver {command}',
                        timeout=30,
                        su=True,
                        exc=True,
                        stderr=None,
                        stdout=None
                    )
            except exceptions.BoxCommandError as e:
                if exc:
                    raise exceptions.ServerError(f"Unable to {command} Server.", original_exception=e)
                else:
                    return False
            except Exception as e:
                raise exceptions.ServerError(f"Unable to {command} Server.", original_exception=e)

            if not exc:
                return True

        def restart(self, exc=False):
            return self.execute_service_command('restart', exc)

        def start(self, exc=False):
            return self.execute_service_command('start', exc)

        def stop(self, exc=False):
            return self.execute_service_command('stop', exc)

        def crash_reason(self):
            if self.is_up():
                return None

            res = self.device.sh(f"dmesg | grep 'oom_reaper.*{self.pid}'")

            if res and res.return_code == 0:
                return 'oom'

            return 'undefined'

        def override_ini_config(self, features):
            storages = BoxPlatform.get_storages_map(self.device)
            if not storages:
                raise BoxCommandError('Unable to get box storages.')

            uid = self.uid()
            base_dir = '/etc'

            if uid != 0:
                username = self.device.eval(f'id -u {uid} -n', su=True)
                homedir = self.device.eval(f'echo ~{username}', su=True)
                base_dir = f'{homedir}/.config'

            ini_dir_path = f'{base_dir}/nx_ini'

            self.device.sh(f'install -m 755 -o {uid} -d "{ini_dir_path}"', exc=True, su=True)

            if len([v for k, v in storages.items() if v['point'] == '/etc/nx_ini']) != 0:
                self.device.sh(f'umount "{ini_dir_path}"', su=True)
                # TODO: check error

            tmp_dir = self.device.eval('mktemp -d --suffix -nx_ini')

            if not tmp_dir:
                raise BoxCommandError(f'Unable to create temp dir at the box via "mktemp".')

            self.device.eval(f'chmod 777 {tmp_dir}', su=True)

            if uid != 0:
                self.device.sh(f'chown {uid} "{tmp_dir}"', exc=True, su=True)

            for ininame, opts in features.items():
                full_ini_path = f'{tmp_dir}/{ininame}.ini'
                file_content = '\n'.join([f"{str(k)}={str(v)}" for k, v in opts.items()]) + '\n'

                self.device.sh(f'cat > "{full_ini_path}"', stdin=file_content, exc=True, su=True)
                if uid != 0:
                    self.device.sh(f'chown {uid} "{full_ini_path}"', exc=True, su=True)

            self.device.sh(f'mount -o bind "{tmp_dir}" "{ini_dir_path}"', exc=True, su=True)

        @staticmethod
        def server_bin(linux_distribution):
            return 'mediaserver-bin' if linux_distribution.with_systemd else 'mediaserver'

    def __init__(self, vmses):
        self.vmses = vmses

    @staticmethod
    def scan(device, linux_distribution):

        if linux_distribution.with_systemd:
            systemd_scripts = device.eval('systemctl list-unit-files --type=service "*"-mediaserver.service', stderr=None)

            if not systemd_scripts:
                return None

            customizations = []
            for line in systemd_scripts.splitlines():
                [customization, *_] = line.rpartition('-mediaserver.service')
                if customization:
                    customizations.append(customization)
        else:
            initd_scripts = device.eval('cd /etc/init.d; ls *-mediaserver', stderr=None)

            if not initd_scripts:
                return None

            customizations = []
            for line in initd_scripts.splitlines():
                [customization, *_] = line.rpartition('-')
                if customization:
                    customizations.append(customization)

        logging.info("Detected services: %s", pformat(customizations))

        vms_descriptions = [{"customization": customization} for customization in customizations]

        for vms in vms_descriptions:
            vms_dir = f"/opt/{vms['customization']}/mediaserver"
            res = device.sh(f"test -d {vms_dir}")
            if res:
                vms['dir'] = vms_dir
            else:
                raise BoxStateError(f'VMS Server dir at the box does not exist: {repr(vms_dir)}.')

            server_config_path = f"{vms['dir']}/etc/mediaserver.conf"
            server_config = device.eval(f"cat {server_config_path}")
            if server_config:
                try:
                    [tmp_file_fd, tmp_file_path] = tempfile.mkstemp()
                except:
                    # TODO: #alevenkov: Rewrite properly.
                    import time
                    time.sleep(100)
                    try:
                        [tmp_file_fd, tmp_file_path] = tempfile.mkstemp()
                    except:
                        raise HostOperationError('Unable to create temp file at the host.')

                tmp_file = open(tmp_file_fd, 'w+b')
                tmp_file.write(server_config.encode())
                tmp_file.close()
                try:
                    vms['port'] = int(ConfigParser(tmp_file_path).options.get('port', 7001))
                except ValueError:
                    raise BoxStateError(f'Unable to get box Server port from {repr(server_config_path)}.')
            else:
                raise BoxStateError(f"VMS Server at {repr(vms['dir'])} has none or empty \"mediaserver.conf\".")

            vms['host'] = device.host

        def obtain_vms_pid(vms_description):
            server_bin = VmsScanner.Vms.server_bin(linux_distribution)
            pids_raw = device.eval(f"pidof {server_bin}")
            from os import environ
            if environ.get('DEBUG', '0') == '1':
                import sys
                print(f"`pidof {server_bin}: {pids_raw}`", file=sys.stderr)
            if not pids_raw:
                return None

            for pid in pids_raw.strip().split():
                bin_dir = device.eval(f"readlink -m /proc/{pid}/cwd", su=True)
                if bin_dir == f"{vms_description['dir']}/bin":
                    return int(pid)
            return None

        vmses = [
            VmsScanner.Vms(
                device=device,
                linux_distribution=linux_distribution,
                customization=description['customization'],
                dir=description['dir'],
                host=description['host'],
                port=description['port'],
                pid=obtain_vms_pid(description)
            )
            for description in vms_descriptions
        ]

        return vmses
