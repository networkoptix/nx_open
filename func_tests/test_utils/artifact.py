'Produce file names for storing artifacts'

import logging
import os.path
import json

from .utils import is_list_inst

log = logging.getLogger(__name__)


class Artifact(object):

    def __init__(self, path_root, ext=None, name=None, full_name=None, is_error=None, type_name=None, content_type=None):
        self.path_root = path_root  # path without extension
        self.ext = ext
        self.name = name
        self.full_name = full_name
        self.is_error = is_error
        self.type_name = type_name
        self.content_type = content_type

    @property
    def path(self):
        return self.path_root + (self.ext or '')


class ArtifactFactory(object):

    @classmethod
    def from_path(cls, db_capture_repository, current_test_run, artifact_set, path):
        return cls(db_capture_repository, current_test_run, artifact_set, Artifact(path))

    def __init__(self, db_capture_repository, current_test_run, artifact_set, artifact):
        self._db_capture_repository = db_capture_repository  # None if junk shop plugin is not installed
        self._current_test_run = current_test_run
        self._artifact_set = artifact_set
        self._artifact = artifact

    def __call__(self, *args, **kw):
        return self.make_artifact(*args, **kw)

    def make_artifact(self, path_part_list=None, ext=None, name=None, full_name=None, is_error=None, type_name=None, content_type=None):
        assert path_part_list is None or is_list_inst(path_part_list, str), repr(path_part_list)
        path_root = '-'.join([self._artifact.path_root] + (path_part_list or []))
        artifact = Artifact(path_root,
                            ext or self._artifact.ext,
                            name or self._artifact.name,
                            full_name or self._artifact.full_name,
                            is_error if is_error is not None else self._artifact.is_error,
                            type_name or self._artifact.type_name,
                            content_type or self._artifact.content_type)
        return ArtifactFactory(self._db_capture_repository, self._current_test_run, self._artifact_set, artifact)

    def produce_file_path(self):
        self._artifact_set.add(self._artifact)
        return self._artifact.path

    def save_json(self, value, encoder=None):
        path = self.make_artifact(ext='.json', type_name='json', content_type='application/json').produce_file_path()
        with open(path, 'w') as f:
            json.dump(value, f, indent=4, cls=encoder)
        return path

    def write_file(self, contents):
        path = self.produce_file_path()
        with open(path, 'w') as f:
            f.write(contents)
        return path

    def release(self):
        repo = self._db_capture_repository
        if not repo: return
        for artifact in sorted(self._artifact_set, key=lambda artifact: artifact.name):
            log.info('Storing artifact: path=%r ext=%r name=%r type_name=%r content_type=%r',
                     artifact.path_root, artifact.ext, artifact.name, artifact.type_name, artifact.content_type)
            if not os.path.exists(artifact.path):
                log.warning('Artifact file is missing, skipping: %s' % artifact.path)
                continue
            with open(artifact.path, 'rb') as f:
                data = f.read()
            at = repo.artifact_type(artifact.type_name, artifact.content_type, artifact.ext)
            repo.add_artifact_with_session(
                self._current_test_run, artifact.name, artifact.full_name or artifact.name, at, data, artifact.is_error or False)
