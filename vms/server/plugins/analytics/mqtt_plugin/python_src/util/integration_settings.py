## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

from dataclasses import dataclass, field
from typing import Optional, Union
import enum
import json


class SettingsItemModelType(enum.Enum):
    TEXT = enum.auto()
    PASSWORD = enum.auto()
    INTEGER = enum.auto()
    BOOLEAN = enum.auto()

    def as_dict(self) -> dict[str, str]:
        if self == SettingsItemModelType.TEXT:
            return {'type': 'TextField'}
        if self == SettingsItemModelType.PASSWORD:
            return {'type': 'PasswordField'}
        if self == SettingsItemModelType.INTEGER:
            return {'type': 'TextField', 'validationRegex': '^[0-9]+$'}
        if self == SettingsItemModelType.BOOLEAN:
            return {'type': 'CheckBox'}

        assert False, f"Unknown type: {self}"
        return ""

SettingsItemModelDict = dict[str, Union[str, bool]]

@dataclass
class SettingsItemModel:
    name: str
    type: SettingsItemModelType = field(default=SettingsItemModelType.TEXT)
    caption: str = ""
    description: str = ""
    default_value: Union[bool, int, str] = ""

    def as_dict(self) -> SettingsItemModelDict:
        return {
            'name': self.name,
            'caption': self.caption,
            'description': self.description,
            'defaultValue': (self.default_value
                            if isinstance(self.default_value, (bool, str))
                            else str(self.default_value)),
            **self.type.as_dict()
        }


@dataclass
class SettingsSectionModel:
    caption: str
    items: list[SettingsItemModel] = field(default_factory=list)

    def as_dict(self) -> dict[str, Union[str, list[SettingsItemModelDict]]]:
        return {
            'caption': self.caption,
            'items': [item.as_dict() for item in self.items]
        }

    def item_type_by_name(self, name: str) -> Optional[SettingsItemModelType]:
        for item in self.items:
            if item.name == name:
                return item.type

        return None


class IntegrationSettingsSection:
    def __init__(self, settings: dict[str, Union[str, int]]):
        self._settings = settings

    def __getattr__(self, attr: str) -> Union[str, int]:
        return self._settings[attr]


class IntegrationSettings:
    def __init__(self, sections: dict[str, SettingsSectionModel]):
        self._sections = sections
        self._settings: dict[str, IntegrationSettingsSection] = {}

    def generate_manifest(self):
        section_manifest_dicts = []
        for section_name, section in self._sections.items():
            section_manifest_dict = {'type': 'GroupBox', **section.as_dict()}
            for item_manifest_dict in section_manifest_dict['items']:
                full_item_name = f"{section_name}_{item_manifest_dict['name']}"
                item_manifest_dict['name'] = full_item_name

            section_manifest_dicts.append(section_manifest_dict)

        return json.dumps({'type': 'Settings', 'items': section_manifest_dicts})

    def __getattr__(self, attr: str) -> IntegrationSettingsSection:
        return self._settings[attr]

    def __bool__(self):
        return bool(self._settings)

    def load_settings(self, settings_dict: dict[str, str]):
        setting_sections = {}
        for name, value in settings_dict.items():
            section_name = next((
                section_name
                for section_name in self._sections.keys()
                if name.startswith(f"{section_name}_")),
                None)
            if not section_name:
                continue

            item_name = name[len(section_name)+1:]
            item_type = self._sections[section_name].item_type_by_name(item_name)
            setting_sections.setdefault(section_name, {})[item_name] = (
                int(value)
                if item_type == SettingsItemModelType.INTEGER
                else value == 'true'
                    if item_type == SettingsItemModelType.BOOLEAN
                    else value)

        self._settings = {n: IntegrationSettingsSection(v) for n, v in setting_sections.items()}
