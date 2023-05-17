#!/usr/bin/env python

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

'''
Script for generating custom stream files for the Object Streamer sub-plugin. See readme.md for
full information.
'''

import argparse
import collections
import copy
import json
import math
import random
import uuid

from abc import ABC, abstractmethod
from pathlib import Path
from typing import Dict, Sequence

class ObjectType:
    '''Analytics Taxonomy Object Type.'''

    def __init__(self):
        self.id = ''
        self.name = ''

    def is_valid(self):
        '''Returns True if both id and name are non-empty, False otherwise.'''
        return self.id and self.name


class ObjectTypeSupportInfo:
    '''Information about supported attributes of an Object Type.'''

    def __init__(self):
        self.object_type_id = ''
        self.attributes: Sequence[str] = []

class ObjectTypeSupportInfoEncoder(json.JSONEncoder):
    '''JSON Encoder for ObjectTypeSupportInfo.'''

    OBJECT_TYPE_ID_KEY = 'objectTypeId'
    ATTRIBUTES_KEY = 'attributes'

    def default(self, o: ObjectTypeSupportInfo) -> Dict:
        result = {}
        result[self.OBJECT_TYPE_ID_KEY] = o.object_type_id
        result[self.ATTRIBUTES_KEY] = o.attributes
        return result

class ObjectTypeInfo:
    '''Information needed for Device Agent Manifest generation.'''

    def __init__(self):
        self.object_type = ObjectType()
        self.object_type_support_info = ObjectTypeSupportInfo()

class Manifest:
    '''Device Agent Manifest for Object Streamer sub-plugin.'''

    class TypeLibrary:
        '''Type Library section of the Device Agent Manifest. Only "objectTypes" part is available
        now.
        '''

        OBJECT_TYPES_KEY = 'objectTypes'
        TYPE_ID_KEY = 'id'
        TYPE_NAME_KEY = 'name'

        def __init__(self, type_library: Dict = None):
            self.object_types: Sequence[ObjectType] = []

            if not type_library:
                return
            object_types = type_library.get(self.OBJECT_TYPES_KEY)
            if not isinstance(object_types, Sequence):
                return

            for object_type_template in object_types:
                if not isinstance(object_type_template, Dict):
                    continue
                object_type = ObjectType()
                object_type.id = object_type_template.get(self.TYPE_ID_KEY)
                object_type.name = object_type_template.get(self.TYPE_NAME_KEY)

                if object_type.is_valid():
                    self.object_types.append(object_type)

    def __init__(self):
        self.type_library = self.TypeLibrary()
        self.supported_types: Sequence[ObjectTypeSupportInfo] = []

class TypeLibraryEncoder(json.JSONEncoder):
    '''JSON Encoder for Manifest.'''

    OBJECT_TYPES_KEY = 'objectTypes'

    def default(self, o: Manifest.TypeLibrary) -> Dict:
        result = {}
        result[self.OBJECT_TYPES_KEY] = [object_type.__dict__ for object_type in o.object_types]
        return result

class ManifestEncoder(json.JSONEncoder):
    '''JSON Encoder for Manifest.'''

    TYPE_LIBRARY_KEY = 'typeLibrary'
    SUPPORTED_TYPES_KEY = 'supportedTypes'

    def default(self, o: Manifest) -> Dict:
        result = {}
        result[self.TYPE_LIBRARY_KEY] = TypeLibraryEncoder().default(o.type_library)
        result[self.SUPPORTED_TYPES_KEY] = [
            ObjectTypeSupportInfoEncoder().default(object_type_support_info)
                for object_type_support_info in o.supported_types]

        return result

class BoundingBox:
    '''Bounding box of an Object on a video frame.'''

    def __init__(self,
            x: float = -1.0,
            y: float = -1.0,
            width: float = -1.0,
            height: float = -1.0):
        self.x = x
        self.y = y
        self.width = width
        self.height = height

    @property
    def left(self) -> float:
        '''Left bound of the bounding box.'''
        return self.x

    @property
    def right(self) -> float:
        '''Right bound of the bounding box.'''
        return self.x + self.width

    @property
    def top(self) -> float:
        '''Top bound of the bounding box.'''
        return self.y

    @property
    def bottom(self) -> float:
        '''Bottom bound of the bounding box.'''
        return self.y + self.height

class ObjectPosition:
    '''Information about position of an Object on a certain video frame.'''

    def __init__(self):
        self.type_id = ''
        self.track_id = str(uuid.uuid4())
        self.bounding_box = BoundingBox()
        self.attributes = {}

    def is_valid(self):
        '''Position is valid if coordinates of all corners of its bounding box is in the range
        [0, 1].
        '''
        return (self.bounding_box.left >= 0.0 and
            self.bounding_box.right <= 1.0 and
            self.bounding_box.top >= 0.0 and
            self.bounding_box.bottom <= 1.0)

class ObjectMovementParameters:
    '''Movement parameters of an Object on a certain video frame.'''

    DEFAULT_SPEED = 0.01

    def __init__(self):
        self.direction: float = 0.0
        self.speed: float = self.DEFAULT_SPEED

class ObjectContext:
    '''Full information about an Object on a certain video frame.'''

    def __init__(self):
        self.frame_number = 0
        self.object_position = ObjectPosition()
        self.object_movement_parameters = ObjectMovementParameters()

class BestShot:
    '''Information about Object Track Best Shot.'''

    def __init__(self):
        self.attributes = {}
        self.image_source = ''

class StreamEntry:
    '''Single entry of the stream file.'''

    REGULAR_ENTRY_TYPE = 'regular'
    BEST_SHOT_ENTRY_TYPE = 'bestShot'
    DEFAULT_ENTRY_TYPE = REGULAR_ENTRY_TYPE

    def __init__(self, frame_number: int, object_position: ObjectPosition):
        self.frame_number = frame_number
        self.entry_type = self.DEFAULT_ENTRY_TYPE
        self.image_source = ''
        self.object_position = object_position

class StreamEntryEncoder(json.JSONEncoder):
    '''JSON Encoder for StreamEntry.'''

    FRAME_NUMBER_KEY = 'frameNumber'
    TYPE_ID_KEY = 'typeId'
    TRACK_ID_KEY = 'trackId'
    ATTRIBUTES_KEY = 'attributes'
    BOUNDING_BOX_KEY = 'boundingBox'
    TOP_LEFT_X_KEY = 'x'
    TOP_LEFT_Y_KEY = 'y'
    WIDTH_KEY = 'width'
    HEIGHT_KEY = 'height'
    IMAGE_SOURCE_KEY = 'imageSource'
    ENTRY_TYPE_KEY = 'entryType'

    def default(self, o: StreamEntry) -> Dict:
        result = {}
        result[self.FRAME_NUMBER_KEY] = o.frame_number
        result[self.TYPE_ID_KEY] = o.object_position.type_id
        result[self.TRACK_ID_KEY] = o.object_position.track_id
        result[self.ATTRIBUTES_KEY] = o.object_position.attributes
        result[self.BOUNDING_BOX_KEY] = {}
        result[self.BOUNDING_BOX_KEY][self.TOP_LEFT_X_KEY] = o.object_position.bounding_box.x
        result[self.BOUNDING_BOX_KEY][self.TOP_LEFT_Y_KEY] = o.object_position.bounding_box.y
        result[self.BOUNDING_BOX_KEY][self.WIDTH_KEY] = o.object_position.bounding_box.width
        result[self.BOUNDING_BOX_KEY][self.HEIGHT_KEY] = o.object_position.bounding_box.height

        if o.entry_type == StreamEntry.BEST_SHOT_ENTRY_TYPE:
            result[self.IMAGE_SOURCE_KEY] = o.image_source
            result[self.ENTRY_TYPE_KEY] = StreamEntry.BEST_SHOT_ENTRY_TYPE

        return result

class Generator(ABC):
    '''Abstract class for Object Track generators.'''

    @abstractmethod
    def generate_object(self) -> ObjectContext:
        '''Generates a new Object.'''

    @abstractmethod
    def move_object(self, object_context: ObjectContext) -> ObjectContext:
        '''Moves an existing Object.'''

    @abstractmethod
    def generate_best_shot(self, frame_number: int, object_context: ObjectContext) -> StreamEntry:
        '''Generates Object Track Best Shot.'''

    @abstractmethod
    def object_type_info(self) -> ObjectTypeInfo:
        '''Returns information about Object Type of the current Object Track.'''

class RandomMovementGenerator(Generator):
    ''' Generates object with a random trajectory, speed and initial coordinates.'''

    class Config:
        '''Random Generator configuration.'''

        FIXED_TRACK_ID_POLICY = 'fixed'
        CHANGE_ON_DEVICE_AGENT_CREATION_TRACK_ID_POLICY = 'changeOnDeviceAgentCreation'
        CHANGE_ONCE_PER_STREAM_CYCLE_TRACK_ID_POLICY = 'changeOncePerStreamCycle'

        TRACK_ID_POLICY_KEY = 'trackIdPolicy'
        OBJECT_TYPE_ID_KEY = 'objectTypeId'
        MIN_WIDTH_KEY = 'minWidth'
        MIN_HEIGHT_KEY = 'minHeight'
        MAX_WIDTH_KEY = 'maxWidth'
        MAX_HEIGHT_KEY = 'maxHeight'
        ATTRIBUTES_KEY = 'attributes'
        BEST_SHOTS_KEY = 'bestShots'
        IMAGE_SOURCE_KEY = 'imageSource'
        FRAME_NUMBER_KEY = 'frameNumber'

        DEFAULT_TRACK_ID_POLICY = FIXED_TRACK_ID_POLICY
        DEFAULT_MIN_WIDTH = 0.05
        DEFAULT_MIN_HEIGHT = 0.05
        DEFAULT_MAX_WIDTH = 0.3
        DEFAULT_MAX_HEIGHT = 0.3

        AUTOMATIC_TYPE_ID_PREFIX = 'stub.objectStreamer.custom.'

        def __init__(self, config):
            object_type_id = config.get(self.OBJECT_TYPE_ID_KEY)
            if object_type_id:
                self.object_type_id = object_type_id
            else:
                self.object_type_id = self.AUTOMATIC_TYPE_ID_PREFIX + str(uuid.uuid4())

            self.track_id_policy = (config.get(self.TRACK_ID_POLICY_KEY)
                if config.get(self.TRACK_ID_POLICY_KEY) is not None
                else self.DEFAULT_TRACK_ID_POLICY)
            self.min_width = (config.get(self.MIN_WIDTH_KEY)
                if config.get(self.MIN_WIDTH_KEY) is not None else self.DEFAULT_MIN_WIDTH)
            self.min_height = (config.get(self.MIN_HEIGHT_KEY)
                if config.get(self.MIN_HEIGHT_KEY) is not None else self.DEFAULT_MIN_HEIGHT)
            self.max_width = (config.get(self.MAX_WIDTH_KEY)
                if config.get(self.MAX_WIDTH_KEY) is not None else self.DEFAULT_MAX_WIDTH)
            self.max_height = (config.get(self.MAX_HEIGHT_KEY)
                if config.get(self.MAX_HEIGHT_KEY) is not None else self.DEFAULT_MAX_HEIGHT)
            self.attributes = (config.get(self.ATTRIBUTES_KEY)
                if config.get(self.ATTRIBUTES_KEY) is not None else {})
            self.best_shots_by_frame_number = {}

            best_shots = config.get(self.BEST_SHOTS_KEY)
            if isinstance(best_shots, collections.abc.Sequence):
                for entry in best_shots:
                    frame_number = entry.get(self.FRAME_NUMBER_KEY)
                    if not isinstance(frame_number, int):
                        continue
                    best_shot = BestShot()
                    best_shot.attributes = entry.get(self.ATTRIBUTES_KEY)
                    best_shot.image_source = entry.get(self.IMAGE_SOURCE_KEY)
                    self.best_shots_by_frame_number[frame_number] = best_shot

    def __init__(self, config: Dict, generator_id: int):
        self._config = self.Config(config)
        self._generator_id = generator_id

    def generate_object(self) -> ObjectContext:
        '''Overrides Generator.generate_object.'''
        # pylint: disable=unused-argument
        object_context = ObjectContext()
        movement_parameters = object_context.object_movement_parameters
        movement_parameters.direction = random.uniform(0, 2 * math.pi)

        position = object_context.object_position
        track_id_policy = self._config.track_id_policy
        if track_id_policy == self.Config.CHANGE_ON_DEVICE_AGENT_CREATION_TRACK_ID_POLICY:
            position.track_id = f'${self._generator_id}'
        elif track_id_policy == self.Config.CHANGE_ONCE_PER_STREAM_CYCLE_TRACK_ID_POLICY:
            position.track_id = f'$${self._generator_id}'

        position.type_id = self._config.object_type_id
        position.attributes = self._config.attributes

        bounding_box = position.bounding_box
        bounding_box.width = random.uniform(self._config.min_width, self._config.max_width)
        bounding_box.height = random.uniform(self._config.min_height, self._config.max_height)
        bounding_box.x = random.uniform(0, 1 - bounding_box.width)
        bounding_box.y = random.uniform(0, 1 - bounding_box.height)

        return object_context

    def move_object(self, object_context: ObjectContext) -> ObjectContext:
        ''' Overrides Generator.move_object.'''
        # pylint: disable=unused-argument
        movement_parameters = object_context.object_movement_parameters
        bounding_box = object_context.object_position.bounding_box
        delta_x = movement_parameters.speed * math.cos(movement_parameters.direction)
        delta_y = movement_parameters.speed * math.sin(movement_parameters.direction)
        bounding_box.x = bounding_box.x + delta_x
        bounding_box.y = bounding_box.y + delta_y
        object_context.object_position.bounding_box = bounding_box

        return object_context

    def generate_best_shot(self, frame_number: int, object_context: ObjectContext) -> StreamEntry:
        if frame_number not in self._config.best_shots_by_frame_number:
            return None

        best_shot = self._config.best_shots_by_frame_number.get(frame_number)
        stream_entry = StreamEntry(frame_number, object_context)
        stream_entry.entry_type = StreamEntry.BEST_SHOT_ENTRY_TYPE
        stream_entry.image_source = best_shot.image_source
        stream_entry.object_position = copy.deepcopy(object_context.object_position)
        stream_entry.object_position.attributes = best_shot.attributes
        return stream_entry

    def object_type_info(self) -> ObjectTypeInfo:
        '''Overrides Generator.object_type_info.'''
        object_type_info = ObjectTypeInfo()

        object_type = object_type_info.object_type
        object_type.id = self._config.object_type_id

        object_type_support_info = object_type_info.object_type_support_info
        object_type_support_info.object_type_id = self._config.object_type_id

        for attribute in self._config.attributes:
            object_type_support_info.attributes.append(attribute)

        return object_type_info

class GenerationContext:
    '''Generation context of a single Object Track.'''

    def __init__(self):
        self.object_context = ObjectContext()
        self.track_generator: Generator = None

class GenerationManager:
    '''Manager responsible for generation of the stream file and manifest with the given parameters.
    '''

    RANDOM_MOVEMENT_POLICY = 'random'

    TYPE_LIBRARY_KEY = 'typeLibrary'
    OBJECT_TYPE_LIBRARY_KEY = 'objectTypes'
    OBJECT_TEMPLATES_KEY = 'objectTemplates'
    MOVEMENT_POLICY_KEY = 'movementPolicy'
    OBJECT_COUNT_KEY = 'objectCount'
    STREAM_DURATION_IN_FRAMES_KEY = 'streamDurationInFrames'

    DEFAULT_MOVEMENT_POLICY = RANDOM_MOVEMENT_POLICY
    DEFAULT_OBJECT_COUNT = 5
    DEFAULT_STREAM_DURATION_IN_FRAMES = 300

    def __init__(self, config: Dict):
        self.generation_contexts = []
        self.stream_duration_in_frames = (config.get(self.STREAM_DURATION_IN_FRAMES_KEY)
            if config.get(self.STREAM_DURATION_IN_FRAMES_KEY) is not None else
                self.DEFAULT_STREAM_DURATION_IN_FRAMES)

        self.type_library = (Manifest.TypeLibrary(config.get(self.TYPE_LIBRARY_KEY)) or
            Manifest.TypeLibrary())

        context_count = (config.get(self.OBJECT_COUNT_KEY)
            if config.get(self.OBJECT_COUNT_KEY) is not None else self.DEFAULT_OBJECT_COUNT)

        while context_count > 0:
            for object_template in (config.get(self.OBJECT_TEMPLATES_KEY) or []):
                if context_count <= 0:
                    break

                generation_context = GenerationContext()
                generation_context.track_generator = self._track_generator(
                    object_template, context_count)
                self.generation_contexts.append(generation_context)
                context_count -= 1

    def generate_manifest(self) -> Manifest:
        '''Generates Device Agent Manifest for the Object Streamer sub-plugin.'''
        manifest = Manifest()
        manifest.type_library = copy.deepcopy(self.type_library)

        object_type_ids = set()
        type_library_object_type_ids = set()

        for object_type in manifest.type_library.object_types:
            type_library_object_type_ids.add(object_type.id)

        for context in self.generation_contexts:
            object_type_info = context.track_generator.object_type_info()
            if object_type_info.object_type.id in object_type_ids:
                continue

            if not object_type_info.object_type.id in type_library_object_type_ids:
                object_type = ObjectType()
                object_type.id = object_type_info.object_type.id

                if not object_type_info.object_type.name:
                    object_type.name = self._make_object_type_name_from_id(object_type.id)
                manifest.type_library.object_types.append(object_type)
                type_library_object_type_ids.add(object_type.id)

            object_type_ids.add(object_type_info.object_type.id)
            manifest.supported_types.append(object_type_info.object_type_support_info)
        return manifest

    def generate_stream(self) -> Sequence[StreamEntry]:
        '''Generates stream file for the Object Streamer sub-plugin.'''
        stream = []
        for frame_number in range(self.stream_duration_in_frames):
            objects_in_frame = []
            for context in self.generation_contexts:
                track_generator = context.track_generator

                context.object_context = track_generator.move_object(context.object_context)

                if not context.object_context.object_position.is_valid():
                    context.object_context = track_generator.generate_object()

                objects_in_frame.append(
                    StreamEntry(frame_number,
                        copy.deepcopy(context.object_context.object_position)))

                best_shot_entry = track_generator.generate_best_shot(
                    frame_number, context.object_context)

                if best_shot_entry is not None:
                    objects_in_frame.append(best_shot_entry)

            stream.extend(objects_in_frame)

        return stream

    def _track_generator(self, object_template: Dict, generator_id: int):
        movement_policy = (object_template.get(self.MOVEMENT_POLICY_KEY)
            or self.DEFAULT_MOVEMENT_POLICY)

        if movement_policy == self.RANDOM_MOVEMENT_POLICY:
            return RandomMovementGenerator(object_template, generator_id)
        return None

    def _make_object_type_name_from_id(self, object_type_id: str):
        '''Generates Object Type name from its id if it isn't explicitly defined in the
        corresponding object template in the configuration file of this script.
        '''
        capitalized = []
        for word in object_type_id.split('.'):
            if not word:
                continue
            try:
                uuid.UUID(word)
                capitalized.append(word)
                continue
            except ValueError: # The given word is not a UUID.
                capitalized.append(word[0].upper() + word[1:])
        return ' '.join(capitalized)

def parse_arguments():
    # pylint: disable=missing-function-docstring
    parser = argparse.ArgumentParser(
        description='Generate an object stream for the Stub Object Streamer sub-plugin')
    parser.add_argument('-c', '--config', type=Path, help='Configuration file for this script')
    parser.add_argument('-m', '--manifest-file', dest='manifest_file',
        nargs='?', default='manifest.json', const='manifest.json', type=Path,
        help='Output manifest file')
    parser.add_argument('-s', '--stream-file', dest='stream_file',
        nargs='?', default='stream.json', const='stream.json', type=Path,
        help='Output stream file')
    parser.add_argument('--compressed-stream', dest='compressed_stream', action='store_true',
        help='Disables formatting of the generated stream file')
    return parser.parse_args()

def load_config(config_file: Path):
    # pylint: disable=missing-function-docstring
    with open(config_file, encoding='UTF-8') as config:
        return json.load(config)

def main():
    # pylint: disable=missing-function-docstring
    args = parse_arguments()
    config = load_config(args.config)
    generation_manager = GenerationManager(config)

    manifest = generation_manager.generate_manifest()
    with open(args.manifest_file, 'w', encoding='UTF-8') as manifest_file:
        json.dump(manifest, manifest_file, cls=ManifestEncoder, indent=4)

    stream = generation_manager.generate_stream()
    with open(args.stream_file, 'w', encoding='UTF-8') as stream_file:
        indent = None if args.compressed_stream else 4
        separators = (",", ":") if args.compressed_stream else (", ", ": ")
        json.dump(stream, stream_file, cls=StreamEntryEncoder, indent=indent, separators=separators)

if __name__ == '__main__':
    main()
