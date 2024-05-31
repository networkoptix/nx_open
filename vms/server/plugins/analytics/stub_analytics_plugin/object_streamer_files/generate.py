#!/usr/bin/env python

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

'''
Script for generating custom stream files for the Object Streamer sub-plugin. See readme.md for
full information.
'''


import argparse
import collections
import copy
import dataclasses
import json
import math
import random
import uuid


from abc import ABC, abstractmethod
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Optional, Sequence


@dataclass
class Attribute:
    '''Analytics Taxonomy Attribute.'''

    name: str
    type: str
    subtype: Optional[str] = None

    def is_valid(self):
        return self.name and self.type


@dataclass
class ObjectType:
    '''Analytics Taxonomy Object Type.'''

    id: str
    name: str
    attributes: Sequence[Attribute] = ()

    def is_valid(self):
        '''Returns True if both id and name are non-empty, False otherwise.'''
        return self.id and self.name


@dataclass
class ObjectTypeSupportInfo:
    '''Information about supported attributes of an Object Type.'''

    objectTypeId: str
    attributes: Sequence[str]


@dataclass
class TypeLibrary:
    '''Type Library section of the Device Agent Manifest. Only "objectTypes" part is available now.
    '''

    objectTypes: Sequence[ObjectType] = ()

    @staticmethod
    def from_json(json: Dict):
        library = TypeLibrary([])

        for template in json.get('objectTypes', []):
            if isinstance(template, Dict):
                object_type = ObjectType(**template)

                object_type.attributes = []
                for item in template.get('attributes', []):
                    if isinstance(item, Dict):
                        attribute = Attribute(**item)
                        if attribute.is_valid():
                            object_type.attributes.append(attribute)

                if object_type.is_valid():
                    library.objectTypes.append(object_type)

        return library


@dataclass
class Manifest:
    '''Device Agent Manifest for Object Streamer sub-plugin.'''

    typeLibrary: TypeLibrary
    supportedTypes: Sequence[ObjectTypeSupportInfo] = ()


class ManifestEncoder(json.JSONEncoder):
    '''JSON Encoder for Manifest.'''

    def default(self, object):
        if dataclasses.is_dataclass(object):
            return dataclasses.asdict(
                object,
                # This dict_factory strips `null` values from the resulting JSON.
                dict_factory=lambda dict: {k: v for (k, v) in dict if v is not None})

        return super().default(object)


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


class Title:
    '''Information about Object Track Title Text and Title Image.'''

    def __init__(self):
        self.title_text = "Some text"
        self.image_source = ''


class StreamEntry:
    '''Single entry of the stream file.'''

    REGULAR_ENTRY_TYPE = 'regular'
    BEST_SHOT_ENTRY_TYPE = 'bestShot'
    TITLE_ENTRY_TYPE = 'title'
    DEFAULT_ENTRY_TYPE = REGULAR_ENTRY_TYPE

    def __init__(self, frame_number: int, object_position: ObjectPosition):
        self.frame_number = frame_number
        self.entry_type = self.DEFAULT_ENTRY_TYPE
        self.image_source = ''
        self.title_text = ''
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
    TITLE_TEXT_KEY = 'titleText'

    def default(self, o: StreamEntry) -> Dict:
        result = {}

        result[self.TRACK_ID_KEY] = o.object_position.track_id
        result[self.FRAME_NUMBER_KEY] = o.frame_number

        result[self.BOUNDING_BOX_KEY] = {}
        result[self.BOUNDING_BOX_KEY][self.TOP_LEFT_X_KEY] = o.object_position.bounding_box.x
        result[self.BOUNDING_BOX_KEY][self.TOP_LEFT_Y_KEY] = o.object_position.bounding_box.y
        result[self.BOUNDING_BOX_KEY][self.WIDTH_KEY] = o.object_position.bounding_box.width
        result[self.BOUNDING_BOX_KEY][self.HEIGHT_KEY] = o.object_position.bounding_box.height

        if o.entry_type == StreamEntry.TITLE_ENTRY_TYPE:
            result[self.ENTRY_TYPE_KEY] = StreamEntry.TITLE_ENTRY_TYPE
            result[self.IMAGE_SOURCE_KEY] = o.image_source
            result[self.TITLE_TEXT_KEY] = o.title_text
            return result # We don't need to fill anything else for Object Track Titles.

        result[self.TYPE_ID_KEY] = o.object_position.type_id

        result[self.ATTRIBUTES_KEY] = o.object_position.attributes

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
    def generate_title(self, frame_number: int, object_context: ObjectContext) -> StreamEntry:
        '''Generates Object Track Title Text and Title Image.'''

    @abstractmethod
    def object_type_info(self) -> ObjectTypeSupportInfo:
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
        OBJECT_TRACK_TITLES_KEY = 'objectTrackTitles'
        IMAGE_SOURCE_KEY = 'imageSource'
        TITLE_TEXT_KEY = 'titleText'
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
            self.titles_by_frame_number = {}

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

            titles = config.get(self.OBJECT_TRACK_TITLES_KEY)
            if isinstance(titles, collections.abc.Sequence):
                for entry in titles:
                    frame_number = entry.get(self.FRAME_NUMBER_KEY)
                    if not isinstance(frame_number, int):
                        continue
                    title = Title()
                    title.image_source = entry.get(self.IMAGE_SOURCE_KEY)
                    title.title_text = entry.get(self.TITLE_TEXT_KEY)
                    self.titles_by_frame_number[frame_number] = title


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

    def generate_title(self, frame_number: int, object_context: ObjectContext) -> StreamEntry:
        if frame_number not in self._config.titles_by_frame_number:
            return None

        title = self._config.titles_by_frame_number.get(frame_number)

        stream_entry = StreamEntry(frame_number, object_context)
        stream_entry.entry_type = StreamEntry.TITLE_ENTRY_TYPE
        stream_entry.image_source = title.image_source
        stream_entry.object_position = copy.deepcopy(object_context.object_position)

        stream_entry.title_text = title.title_text

        return stream_entry

    def object_type_info(self) -> ObjectTypeSupportInfo:
        return ObjectTypeSupportInfo(self._config.object_type_id, set(self._config.attributes))


class GenerationContext:
    '''Generation context of a single Object Track.'''

    def __init__(self):
        self.object_context = ObjectContext()
        self.track_generator: Generator = None


class GenerationManager:
    '''Manager responsible for generation of the stream file and manifest with the given parameters.
    '''

    RANDOM_MOVEMENT_POLICY = 'random'

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

        self.type_library = TypeLibrary.from_json(config.get('typeLibrary', {}))

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
        manifest = Manifest(copy.deepcopy(self.type_library))

        attributes_by_object_type = {}

        for context in self.generation_contexts:
            info = context.track_generator.object_type_info()
            attributes_by_object_type[info.objectTypeId] = \
                attributes_by_object_type.get(info.objectTypeId, set()).union(info.attributes)

        manifest.supportedTypes = []
        for type_id, attributes in attributes_by_object_type.items():
            manifest.supportedTypes.append(ObjectTypeSupportInfo(type_id, sorted(attributes)))

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

                title_entry = track_generator.generate_title(frame_number, context.object_context)

                if title_entry is not None:
                    objects_in_frame.append(title_entry)

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
    parser.add_argument('-c', '--config', default='config.json', type=Path,
        help='Configuration file for this script')
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
        manifest_file.write("\n")

    stream = generation_manager.generate_stream()
    with open(args.stream_file, 'w', encoding='UTF-8') as stream_file:
        indent = None if args.compressed_stream else 4
        separators = (",", ":") if args.compressed_stream else (",", ": ")
        json.dump(stream, stream_file, cls=StreamEntryEncoder, indent=indent, separators=separators)
        stream_file.write("\n")


if __name__ == '__main__':
    main()
