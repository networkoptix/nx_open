from contextlib import closing

from debian.debfile import DebFile
from pathlib2 import PurePosixPath

from test_utils.build_info import customizations_from_paths, build_info_from_text


class Deb(object):
    def __init__(self, path, installation_root=PurePosixPath('/opt')):
        self.path = path
        deb_file = DebFile(str(path)).data.tgz()
        tar_members = deb_file.getmembers()
        paths = [PurePosixPath('/', member.path) for member in tar_members]
        self.customization, = customizations_from_paths(paths, installation_root)
        build_info_path = installation_root / self.customization.installation_subdir / 'build_info.txt'
        with closing(deb_file.extractfile('.' + str(build_info_path))) as build_info_file:  # Pathlib omits dot.
            build_info_data = build_info_file.read()
        self.build_info = build_info_from_text(build_info_data)
