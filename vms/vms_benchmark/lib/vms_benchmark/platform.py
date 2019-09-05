import re
from functools import reduce
from vms_benchmark.utils import human_readable_size


class Platform:
    def __init__(self, device, ram, arch, cpu_number, cpu_features, storages_list):
        self.device = device
        self.ram = ram
        self.arch = arch
        self.cpu_number = cpu_number
        self.cpu_features = cpu_features
        self.storages_list = storages_list

    def ram_free(self):
        meminfo = self.device.get_file_content('/proc/meminfo')

        if meminfo is None:
            return None

        meminfo_parsed = dict(
            (part.strip() for part in line.split(':')) for line in meminfo.split('\n') if ':' in line
        )

        ram_free = int(human_readable_size.human2bytes(meminfo_parsed['MemFree']))

        return ram_free

    def ram_available(self):
        meminfo = self.device.get_file_content('/proc/meminfo', timeout=10)

        if meminfo is None:
            return None

        meminfo_parsed = dict(
            (part.strip() for part in line.split(':')) for line in meminfo.split('\n') if ':' in line
        )

        ram_available = int(human_readable_size.human2bytes(meminfo_parsed['MemAvailable']))

        return ram_available

    @staticmethod
    def gather(device):
        # Detect memory capacity:
        meminfo = device.get_file_content('/proc/meminfo')

        if meminfo is None:
            return None

        meminfo_parsed = dict(
            (part.strip() for part in line.split(':')) for line in meminfo.split('\n') if ':' in line
        )

        ram = human_readable_size.human2bytes(meminfo_parsed['MemTotal'])

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

        fs_filters = [
            'ext2',
            'ext3',
            'ext4',
            'ntfs',
            'vfat'
        ]

        storage_is_writable = lambda storage: 'rw' in storage['flags'] and storage['flags']['rw']

        def build_storages(storages, storage):
            storages[storage['point']] = storage
            return storages

        storages_list = reduce(
            lambda storages, storage: build_storages(storages, storage),
            [
                storage
                for storage in
                [
                    construct_mount_description(line)
                    for line in mounts.split('\n')
                ]
                if storage_is_writable(storage) and storage['fs'] in fs_filters
            ],
            {}
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

            if volume['point'] in storages_list:
                storages_list[volume['point']]['space_total'] = volume['space_total']
                storages_list[volume['point']]['space_free'] = volume['space_free']

        [
            construct_df_description(line)
            for line in
            df_data.split('\n')[1:]
        ]

        return Platform(
            device=device,
            ram=ram,
            arch=arch,
            cpu_number=len(cpuinfo_parsed),
            cpu_features=cpu_features,
            storages_list=storages_list
        )
