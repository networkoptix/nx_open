from contextlib import closing

from debian.arfile import ArError
from debian.debfile import DebFile
from pathlib2 import PurePosixPath

from framework.build_info import build_info_from_text, customizations_from_paths


class MediaserverDeb(object):
    class InvalidDeb(Exception):
        pass

    def __init__(self, path, installation_root=PurePosixPath('/opt')):
        self.path = path
        try:
            deb_file = DebFile(str(self.path)).data.tgz()
        except ArError:
            raise self.InvalidDeb("File {} is not a valid DEB package".format(self.path))
        tar_members = deb_file.getmembers()
        paths = [PurePosixPath('/', member.path) for member in tar_members]
        customizations = customizations_from_paths(paths, installation_root)
        try:
            self.customization, = customizations
        except ValueError:
            raise Exception("Expected exactly one mediaserver dir in {}".format(self.path))
        build_info_path = installation_root / self.customization.installation_subdir / 'build_info.txt'
        with closing(deb_file.extractfile('.' + str(build_info_path))) as build_info_file:  # Pathlib omits dot.
            build_info_data = build_info_file.read()
        self.build_info = build_info_from_text(build_info_data)
        if self.build_info.customization != self.customization.name:
            raise self.InvalidDeb("Customization detected from paths doesn't match customization from build_info.txt")
