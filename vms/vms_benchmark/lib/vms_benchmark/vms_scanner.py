from re import sub as re_sub
import logging
import tempfile
from pprint import pformat

from vms_benchmark import exceptions
from vms_benchmark.box_platform import BoxPlatform
from vms_benchmark.config import ConfigParser
from vms_benchmark.exceptions import BoxCommandError, BoxStateError, HostOperationError

ini_ssh_service_command_timeout_s: int


class VmsScanner:
    class Vms:
        _tmp_dir_suffix = '-nx_ini'

        def __init__(self, device, linux_distribution, pid, customization, service_script, dir, host, port, uid, ini_dir):
            self.customization = customization
            self.service_script = service_script
            self.dir = dir
            self.host = host
            self.port = port
            self.device = device
            self.linux_distribution = linux_distribution
            self.pid = pid
            self.uid = uid
            self.ini_dir = ini_dir

        def is_up(self):
            pids_raw = self.device.eval(f'pidof {self.server_bin(self.linux_distribution)}', stderr=None)
            if not pids_raw:
                return False

            return self.pid in [int(pid) for pid in pids_raw.strip().split()]

        def execute_service_command(self, command, exc=False):
            try:
                if self.linux_distribution.with_systemd:
                    self.device.sh(
                        f'systemctl {command} {self.service_script}',
                        timeout_s=ini_ssh_service_command_timeout_s,
                        su=True,
                        exc=True,
                        stderr=None,
                        stdout=None
                    )
                else:
                    self.device.sh(
                        f'/etc/init.d/{self.service_script} {command}',
                        timeout_s=ini_ssh_service_command_timeout_s,
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

        def dismount_ini_dirs(self):
            logging.info("Dismounting ini dirs...")
            try:
                storages = BoxPlatform.get_storages_map(self.device)
                if not storages:
                    raise BoxCommandError('Unable to get box Storages.')
                if len([v for k, v in storages.items() if v['point'] == self.ini_dir]) != 0:
                    self.device.sh(f'umount "{self.ini_dir}"', exc=True, su=True)
                self.device.sh(
                    'rm -rf '
                    f'"$(dirname "$(mktemp --dry-run)")"/tmp.*"{self._tmp_dir_suffix}" '
                    f'"{self.ini_dir}"',
                    exc=True, su=True)
            except Exception:
                logging.exception("Exception while dismounting ini dirs:")

        def override_ini_config(self, features):
            self.device.sh(f'install -m 755 -o {self.uid} -d "{self.ini_dir}"', exc=True, su=True)

            tmp_dir = self.device.eval(f'mktemp -d --suffix {self._tmp_dir_suffix}')

            if not tmp_dir:
                raise BoxCommandError(f'Unable to create temp dir at the box via "mktemp".')

            self.device.eval(f'chmod 777 {tmp_dir}', su=True)

            if self.uid != 0:
                self.device.sh(f'chown {self.uid} "{tmp_dir}"', exc=True, su=True)

            for ininame, opts in features.items():
                full_ini_path = f'{tmp_dir}/{ininame}.ini'
                file_content = '\n'.join([f"{str(k)}={str(v)}" for k, v in opts.items()]) + '\n'

                self.device.sh(f'cat > "{full_ini_path}"', stdin=file_content, exc=True, su=True)
                if self.uid != 0:
                    self.device.sh(f'chown {self.uid} "{full_ini_path}"', exc=True, su=True)

            self.device.sh(f'mount -o bind "{tmp_dir}" "{self.ini_dir}"', exc=True, su=True)

        @staticmethod
        def server_bin(linux_distribution):
            return 'mediaserver-bin' if linux_distribution.with_systemd else 'mediaserver'

    def __init__(self, vmses):
        self.vmses = vmses

    @staticmethod
    def scan(device, linux_distribution):

        if linux_distribution.with_systemd:
            systemd_scripts = device.eval(
                'systemctl list-unit-files --type=service "*"-mediaserver.service',
                stderr=None,
            )

            if not systemd_scripts:
                return None

            customizations = []
            service_scripts = []
            for line in systemd_scripts.splitlines():
                [service_script, *_] = line.rpartition(' ')
                [customization, *_] = service_script.rpartition('-mediaserver.service')
                if customization:
                    service_scripts.append(service_script)
                    customizations.append(customization)
        else:
            initd_scripts = device.eval('cd /etc/init.d; ls *-mediaserver', stderr=None)

            if not initd_scripts:
                return None

            customizations = []
            service_scripts = []
            # An example of the init script name: "S99digitalwatchdog-mediaserver".
            # To get the customization name, we should remove the right part and strip the prefix.
            for line in initd_scripts.splitlines():
                [customization, *_] = line.rpartition('-')
                if customization:
                    service_scripts.append(line)
                    customizations.append(re_sub(r'^(S\d{2})?', '', customization))

        vms_descriptions = [
            {"service_script": s, "customization": c}
            for s, c in zip(service_scripts, customizations)
        ]

        logging.info("Detected services: %s", pformat(vms_descriptions))

        for vms in vms_descriptions:
            vms_dir = f"/opt/{vms['customization']}/mediaserver"
            res = device.sh(f"test -d {vms_dir}")
            if res:
                vms['dir'] = vms_dir
            else:
                raise BoxStateError(f'VMS Server dir at the box does not exist: {vms_dir!r}.')

            server_config_path = f"{vms['dir']}/etc/mediaserver.conf"
            server_config = device.eval(f"cat {server_config_path}")
            if server_config:
                try:
                    [tmp_file_fd, tmp_file_path] = tempfile.mkstemp()
                except Exception:
                    # TODO: #alevenkov: Rewrite properly.
                    import time
                    time.sleep(0.1)
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
                    raise BoxStateError(f'Unable to get box Server port from {server_config_path!r}.')
            else:
                raise BoxStateError(f"VMS Server at {vms['dir']!r} has none or empty \"mediaserver.conf\".")

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
                # `readlink -m` may not work on busybox.
                bin_dir = device.eval(f"readlink -f /proc/{pid}/cwd", su=True)

                # Server installation directory in /opt/ can be a symlink, so to compare it with
                # the working directory of the running Server process, we should get the "real"
                # path first.
                install_dir = device.eval(f"readlink -f '{vms_description['dir']}/bin'", su=True)

                if bin_dir == install_dir:
                    return int(pid)
            return None

        def obtain_uid(pid):
            if pid is None:
                return None
            proc_status_filename = f'/proc/{pid}/status'
            uid_str = device.eval(
                fr"cat {proc_status_filename} | grep -o 'Uid:\s\+[0-9]\+' | grep -o '[0-9]\+'",
                su=True
            )
            if uid_str is None:
                raise exceptions.BoxCommandError(f"Cannot obtain uid of running Server.")
            try:
                return int(uid_str)
            except ValueError:
                raise exceptions.BoxFileContentError(proc_status_filename)

        def obtain_ini_dir(uid):
            if uid is None:
                return None

            base_dir = '/etc'

            if uid != 0:
                username = device.eval(f'id -u {uid} -n', su=True)
                homedir = device.eval(f'echo ~{username}', su=True)
                base_dir = f'{homedir}/.config'

            return f'{base_dir}/nx_ini'

        vmses = []
        for description in vms_descriptions:
            pid = obtain_vms_pid(description)
            uid = obtain_uid(pid)
            ini_dir = obtain_ini_dir(uid)

            vmses.append(VmsScanner.Vms(
                device=device,
                linux_distribution=linux_distribution,
                customization=description['customization'],
                service_script=description['service_script'],
                dir=description['dir'],
                host=description['host'],
                port=description['port'],
                pid=pid,
                uid=uid,
                ini_dir=ini_dir,
            ))

        return vmses
