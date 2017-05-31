'Produce file names for storing artifacts'

import os.path


class ArtifactFactory(object):

    def __init__(self, artifact_path_prefix):
        self._artifact_path_prefix = artifact_path_prefix

    def __call__(self, *name_part_list):
        path = self._artifact_path_prefix
        for part in name_part_list:
            if part.startswith('.'):
                path += part
            else:
                path += '-' + part
        return path
