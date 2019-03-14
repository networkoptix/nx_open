import tarfile
import zipfile


class Archiver():
    def __init__(self, file_name):
        self._file_name = file_name

        if file_name.endswith(".zip"):
            self._file = zipfile.ZipFile(file_name, mode="w", compression=zipfile.ZIP_DEFLATED)
            self.add = self._add_to_zip
        elif file_name.endswith(".tar.gz"):
            self._file = tarfile.open(file_name, mode="w:gz", encoding="utf-8")
            self.add = self._add_to_tar
        else:
            raise Exception("Unsupported archive format: %s" % file_name)

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self._file.close()

    def _add_to_zip(self, file_name, archive_name):
        self._file.write(file_name, archive_name)

    def _add_to_tar(self, file_name, archive_name):
        self._file.add(file_name, archive_name)
