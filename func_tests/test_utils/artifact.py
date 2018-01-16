'Produce file names for storing artifacts'

import logging
import os.path
import json

from .utils import is_list_inst

log = logging.getLogger(__name__)


class ArtifactType(object):

    def __init__(self, name, content_type=None, ext=None):
        self.name = name
        self.content_type = content_type
        self.ext = ext

    def produce_repository_type(self, repository):
        return repository.artifact_type(self.name, self.content_type, self.ext)


class Artifact(object):

    def __init__(self, path_root, name=None, full_name=None, is_error=None, artifact_type=None):
        self.path_root = path_root  # path without extension
        self.name = name
        self.full_name = full_name
        self.is_error = is_error
        self.artifact_type = artifact_type

    @property
    def path(self):
        path = self.path_root
        if self.artifact_type and self.artifact_type.ext:
            return path + self.artifact_type.ext
        else:
            return path


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

    def make_artifact(self, path_part_list=None, name=None, full_name=None, is_error=None, artifact_type=None):
        assert path_part_list is None or is_list_inst(path_part_list, str), repr(path_part_list)
        assert artifact_type is None or isinstance(artifact_type, ArtifactType), repr(artifact_type)
        path_root = '-'.join([self._artifact.path_root] + (path_part_list or []))
        artifact = Artifact(
            path_root,
            name or self._artifact.name,
            full_name or self._artifact.full_name,
            is_error if is_error is not None else self._artifact.is_error,
            artifact_type or self._artifact.artifact_type,
            )
        return ArtifactFactory(self._db_capture_repository, self._current_test_run, self._artifact_set, artifact)

    def produce_file_path(self):
        self._artifact_set.add(self._artifact)
        return self._artifact.path

    def save_as_json(self, value, encoder=None):
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
        repository = self._db_capture_repository
        if not repository: return
        for artifact in sorted(self._artifact_set, key=lambda artifact: artifact.name):
            assert artifact.artifact_type, repr(artifact.name)
            log.info('Storing artifact: path=%r name=%r ext=%r type_name=%r content_type=%r',
                     artifact.path_root, artifact.name,
                     artifact.artifact_type.ext, artifact.artifact_type.name, artifact.artifact_type.content_type)
            if not os.path.exists(artifact.path):
                log.warning('Artifact file is missing, skipping: %s' % artifact.path)
                continue
            with open(artifact.path, 'rb') as f:
                data = f.read()
            repository_artifact_type = artifact.artifact_type.produce_repository_type(repository)
            repository.add_artifact_with_session(
                self._current_test_run,
                artifact.name,
                artifact.full_name or artifact.name,
                repository_artifact_type,
                data,
                artifact.is_error or False,
                )
