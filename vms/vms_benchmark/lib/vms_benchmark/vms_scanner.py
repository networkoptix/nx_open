import tempfile
from vms_benchmark import exceptions
from vms_benchmark.config import ConfigParser
from vms_benchmark.box_platform import BoxPlatform


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

        def restart(self, exc=False):
            try:
                if self.linux_distribution.with_systemd:
                    self.device.sh(
                        f'systemctl restart {self.customization}-mediaserver',
                        timeout=30,
                        su=True,
                        exc=True,
                        stderr=None,
                        stdout=None
                    )
                else:
                    self.device.sh(
                        f'/etc/init.d/{self.customization}-mediaserver restart',
                        timeout=30,
                        su=True,
                        exc=True,
                        stderr=None,
                        stdout=None
                    )
            except exceptions.BoxCommandError as e:
                if exc:
                    raise exceptions.ServerError("Unable to restart Server.", original_exception=e)
                else:
                    return False
            except Exception as e:
                raise exceptions.ServerError("Unable to restart Server.", original_exception=e)

            if not exc:
                return True

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
                return False

            uid = self.uid()
            base_dir = '/etc'

            if uid != 0:
                username = self.device.eval(f'id -u {uid} -n', exc=True, su=True)
                homedir = self.device.eval(f'echo ~{username}', exc=True, su=True)
                base_dir = f'{homedir}/.config'

            ini_dir_path = f'{base_dir}/nx_ini'

            self.device.sh(f'install -m 755 -o {uid} -d "{ini_dir_path}"', exc=True, su=True)

            if len([v for k, v in storages.items() if v['point'] == '/etc/nx_ini']) != 0:
                self.device.sh(f'umount "{ini_dir_path}"', su=True)
                # TODO: check error

            tmp_dir = self.device.eval('mktemp -d --suffix -nx_ini')

            if not tmp_dir:
                pass
                # TODO: process the error

            if uid != 0:
                self.device.sh(f'chown {uid} "{tmp_dir}"', exc=True, su=True)

            for ininame, opts in features.items():
                full_ini_path = f'{tmp_dir}/{ininame}.ini'
                file_content = '\n'.join([f"{str(k)}={str(v)}" for k, v in opts.items()]) + '\n'

                self.device.sh(f'cat > "{full_ini_path}"', stdin=file_content, exc=True, su=True)
                if uid != 0:
                    self.device.sh(f'chown {uid} "{full_ini_path}"', exc=True, su=True)

            if not self.device.sh(f'mount -o bind "{tmp_dir}" "{ini_dir_path}"', su=True):
                # TODO: process the error
                pass

            return True

        @staticmethod
        def server_bin(linux_distribution):
            return 'mediaserver-bin' if linux_distribution.with_systemd else 'mediaserver'

    def __init__(self, vmses):
        self.vmses = vmses

    @staticmethod
    def scan(device, linux_distribution):
        if linux_distribution.with_systemd:
            systemd_scripts = device.eval('systemctl list-unit-files --type=service *-mediaserver.service', stderr=None)

            if not systemd_scripts:
                return None

            customizations = [
                line.split()[0].split('-')[0]
                for line in systemd_scripts.split('\n')
                if '.service' in line
            ]
        else:
            initd_scripts = device.eval('cd /etc/init.d; ls *-mediaserver', stderr=None)

            if not initd_scripts:
                return None

            customizations = [line.split('-')[0] for line in initd_scripts.split('\n') if len(line.strip()) > 0]

        vms_descriptions = [{"customization": customization} for customization in customizations]

        for vms in vms_descriptions:
            probably_vms_dir = f"/opt/{vms['customization']}/mediaserver"
            res = device.sh(f"test -d {probably_vms_dir}")

            if res:
                vms['dir'] = probably_vms_dir
            else:
                # Directory of the VMS is not found
                vms['dir'] = None

            # TODO: this is a kostyl :)
            server_config = device.eval(f"cat {vms['dir']}/etc/mediaserver.conf")
            if server_config:
                try:
                    tmp_file_fd, tmp_file_path = tempfile.mkstemp()
                except:
                    import time
                    time.sleep(100)

                tf = open(tmp_file_fd, 'w+b')
                tf.write(server_config.encode())
                tf.close()
                vms['port'] = int(ConfigParser(tmp_file_path).options.get('port', 7001))

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
