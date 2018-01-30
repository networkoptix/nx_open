"""Produce file names for storing artifacts."""

import logging
import json

from .utils import is_list_inst

log = logging.getLogger(__name__)


class ArtifactType(object):

    def __init__(self, name, content_type=None, ext=None):
        self.name = name
        self.content_type = content_type
        self.ext = ext

    def __repr__(self):
        return '<name=%r ext=%r content_type=%r>' % (self.name, self.content_type, self.ext)

    def produce_repository_type(self, repository):
        return repository.artifact_type(self.name, self.content_type, self.ext)


JSON_ARTIFACT_TYPE = ArtifactType(name='json', content_type='application/json', ext='.json')


class Artifact(object):

    def __init__(self, path_root, name=None, full_name=None, is_error=None, artifact_type=None):
        self.path_root = path_root  # path without extension
        self.name = name
        self.full_name = full_name
        self.is_error = is_error
        self.artifact_type = artifact_type

    def __repr__(self):
        return '<path=%r name=%r type=%r>' % (self.path_root, self.name, self.artifact_type)

    @property
    def path(self):
        path = self.path_root
        if self.artifact_type and self.artifact_type.ext:
            return path.with_suffix(self.artifact_type.ext)
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
        name = '-'.join([self._artifact.path_root.name] + (path_part_list or []))
        artifact = Artifact(
            self._artifact.path_root.parent / name,
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
        path = self.make_artifact(artifact_type=JSON_ARTIFACT_TYPE).produce_file_path()
        path.write_bytes(json.dumps(value, indent=4, cls=encoder))
        return path

    def release(self):
        repository = self._db_capture_repository
        if not repository:
            return
        for artifact in sorted(self._artifact_set, key=lambda artifact: artifact.name):
            assert artifact.artifact_type, repr(artifact)
            log.info('Storing artifact: %r', artifact)
            if not artifact.path.exists():
                log.warning('Artifact file is missing, skipping: %s' % artifact.path)
                continue
            data = artifact.path.read_bytes
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
