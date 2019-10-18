from functools import reduce

from vms_benchmark.utils import human_readable_size


class BoxPlatform:
    def __init__(self, device, linux_distribution, ram_bytes, arch, cpu_count, cpu_features, storages_list):
        self.device = device
        self.ram_bytes = ram_bytes
        self.arch = arch
        self.cpu_count = cpu_count
        self.cpu_features = cpu_features
        self.storages_list = storages_list
        self.linux_distribution = linux_distribution

    def ram_free_bytes(self):
        meminfo = self.device.get_file_content('/proc/meminfo')

        if meminfo is None:
            return None

        meminfo_parsed = dict(
            (part.strip() for part in line.split(':')) for line in meminfo.split('\n') if ':' in line
        )

        ram_free_bytes = int(human_readable_size.human2bytes(meminfo_parsed['MemFree']))

        return ram_free_bytes

    def ram_available_bytes(self):
        if (
            self.linux_distribution.kernel_version[0] < 3 or
            (
                self.linux_distribution.kernel_version[0] == 3 and
                self.linux_distribution.kernel_version[1] < 14
            )
        ):
            return None

        meminfo = self.device.get_file_content('/proc/meminfo', timeout=10)

        if meminfo is None:
            return None

        meminfo_parsed = dict(
            (part.strip() for part in line.split(':')) for line in meminfo.split('\n') if ':' in line
        )

        ram_available = int(human_readable_size.human2bytes(meminfo_parsed['MemAvailable']))

        return ram_available

    @staticmethod
    def get_storages_map(device):
        mounts = device.get_file_content('/proc/mounts')

        if not mounts:
            return None

        def construct_mount_description(line):
            components = line.split()
            return {
                "device": components[0],
                "point": components[1],
                "fs": components[2],
                "flags": dict(flag.split('=') if '=' in flag else [flag, True] for flag in components[3].split(',')),
                "dump": True if components[4] == '1' else False,
                "check_priority": int(components[5]),
            }

        def build_storages(storages, storage):
            storages[storage['point']] = storage
            return storages

        storages_map = reduce(
            lambda storages, storage: build_storages(storages, storage),
            [
                storage
                for storage in
                [
                    construct_mount_description(line)
                    for line in mounts.split('\n')
                ]
            ],
            {}
        )

        return storages_map

    @staticmethod
    def gather(device, linux_distribution):
        # Detect memory capacity:
        meminfo = device.get_file_content('/proc/meminfo')

        if meminfo is None:
            return None

        meminfo_parsed = dict(
            (part.strip() for part in line.split(':')) for line in meminfo.split('\n') if ':' in line
        )

        ram_bytes = human_readable_size.human2bytes(meminfo_parsed['MemTotal'])

        # Detect architecture:
        arch = device.eval('uname -m')

        # Obtain detailed CPU information:
        cpuinfo = device.get_file_content('/proc/cpuinfo')

        if cpuinfo is None:
            return None

        cpuinfo_parsed = dict(
            (part.strip() for part in line.split(':')) for line in cpuinfo.split('\n') if ':' in line
        )

        cpu_features = []

        if arch in ['aarch64', 'armv7l']:
            cpu_features_raw = cpuinfo_parsed['Features'].split()

            if 'neon' in cpu_features_raw:
                cpu_features.append('NEON')

        # Obtain storages
        storages_map = BoxPlatform.get_storages_map(device)

        if not storages_map:
            return None

        fs_filters = [
            'ext2',
            'ext3',
            'ext4',
            'ntfs',
            'vfat'
        ]

        storage_is_writable = lambda storage: 'rw' in storage['flags'] and storage['flags']['rw']

        storages_map_filtered = dict(
            [k, v]
            for k, v in storages_map.items()
            if storage_is_writable(v) and v['fs'] in fs_filters
        )

        df_data = device.eval('df -B1')

        if not df_data:
            return None

        def construct_df_description(line):
            components = line.split()
            volume = {
                "device": components[0],
                "point": components[5],
                "space_total": components[1],
                "space_free": components[3]
            }

            for (_point, storage) in storages_map_filtered.items():
                if storage['device'] != volume['device']:
                    continue

                storage['space_total'] = volume['space_total']
                storage['space_free'] = volume['space_free']

        [
            construct_df_description(line)
            for line in
            df_data.split('\n')[1:]
        ]

        return BoxPlatform(
            device=device,
            ram_bytes=ram_bytes,
            arch=arch,
            cpu_count=len([line for line in cpuinfo.splitlines() if line.startswith('processor')]),
            cpu_features=cpu_features,
            storages_list=storages_map_filtered,
            linux_distribution=linux_distribution
        )
