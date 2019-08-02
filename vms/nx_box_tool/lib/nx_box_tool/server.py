import tempfile
from nx_box_tool.config import ConfigParser


class Server:
    class Vms:
        def __init__(self, device, pid, customization, dir, host, port):
            self.customization = customization
            self.dir = dir
            self.host = host
            self.port = port
            self.device = device
            self.pid = pid

        def is_up(self):
            pids_raw = self.device.eval('pidof mediaserver-bin')
            if not pids_raw:
                return False

            return True

            return self.pid in [int(pid) for pid in pids_raw.strip().split()]

        def crash_reason(self):
            if self.is_up():
                return None

            res = self.device.sh(f"dmesg | grep 'oom_reaper.*{self.pid}'")

            if res and res.return_code == 0:
                return 'oom'

            return 'undefined'

    def __init__(self, vmses):
        self.vmses = vmses

    @staticmethod
    def detect(device):
        systemd_scripts = device.eval('systemctl list-unit-files --type=service *-mediaserver.service', stderr=None)

        if systemd_scripts is None:
            return None

        vms_descriptions = [
            {
                "customization": line.split('-')[0]
            }
            for line in [
                line.split()[0][:-8]
                for line in systemd_scripts.split('\n')
                if '.service' in line
            ]
        ]

        for vms in vms_descriptions:
            probably_vms_dir = f"/opt/{vms['customization']}/mediaserver"
            res = device.sh(f"test -d {probably_vms_dir}")

            if res:
                vms['dir'] = probably_vms_dir
            else:
                # Directory of the VMS is not found
                vms['dir'] = None

            # TODO: this is a kostil :)
            server_config = device.eval(f"cat {vms['dir']}/etc/mediaserver.conf")
            if server_config:
                tf = tempfile.NamedTemporaryFile()
                tf.write(server_config.encode())
                tf.flush()
                vms['port'] = int(ConfigParser(tf.name).options.get('port', 7001))

            vms['host'] = device.host

        def obtain_vms_pid(vms_description):
            pids_raw = device.eval("pidof mediaserver-bin")
            if not pids_raw:
                return None

            for pid in pids_raw.strip().split():
                bin_dir = device.eval(f"sudo readlink -m /proc/{pid}/cwd")
                if bin_dir == f"{vms_description['dir']}/bin":
                    return int(pid)
            return None

        vmses = [
            Server.Vms(
                device=device,
                customization=description['customization'],
                dir=description['dir'],
                host=description['host'],
                port=description['port'],
                pid=obtain_vms_pid(description)
            )
            for description in vms_descriptions
        ]

        return Server(vmses=vmses)

    def has_vms(self):
        return len(self.vmses) > 0
