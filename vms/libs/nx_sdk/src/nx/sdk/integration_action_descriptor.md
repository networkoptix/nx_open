# Integration Action Descriptor

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

## Integration Action Descriptor

    {
        "id": "vendor.integrationName.integrationActionName",
        "name": "MQTT publish",
        "description": "Publish MQTT message to MQTT server",
        "isInstant": true,
        "fields": [

        ]
    }

### id

Id of the action type. Must be unique across all the Integrations.

### name

Human-readable name of the action. Must be unique across all the Integrations.

## description

The description of the action. Optional

### isInstant

Flag, specifying if the Action is instant (default), or prolonged (if the value of this flag is set
to false).

---------------------------------------------------------------------------------------------------
## Fields

The "Field" object has the following fields:

- `type` - String. Determines the type of the field.
- `fieldName` - String. Name of the field. Must be unique in the scope of the Action.
- `displayName` - String. Name of the field shown to user in the UI. Optional - if omitted,
    `fieldName` is used.
- `description` - String. Description of the field shown in the UI. Optional.
- `properties` - Object. Type-dependent properties of the field. Can be omitted.

### Flag

Type `flag`. Represents boolean value. Does not have `properties`.

### Integer

Type `int`. Represents integer value.

### String selection

Type `stringSelection`. Represents a list of fixed options. `properties` has the following format:

    {
        "items": [
            {"name": "Option 1 Name", "value": "option_1_value"}
            ...
            {"name": "Option N Name", "value": "option_n_value"}
        ]
    }

### Password

Type `password`. Represents secret. Does not have `properties`.

### Devices

Type `devices`. Represents set of devices. `properties` has the following format:

    {
        "acceptAll": false,
        "useSource": false,
        "validationPolicy": "cameraOutput"
    }

"validationPolicy" can be one of "bookmark", "cameraAudioTransmission", "cameraOutput",
"cameraRecording", "cameraFullScreen" or empty.

### Layouts

Type `layouts`. Represents set of layouts. Does not have `properties`.

### Servers

Type `servers`. Represents Servers. `properties` has the following format:

    {
        "acceptAll": false,
        "allowEmptySelection": false,
        "validationPolicy": "hasBuzzer"
    }

"validationPolicy" must be "hasBuzzer".

# Users

Type `users`. Represents users. `properties` has the following format:

    {
        "acceptAll": false,
        "allowEmptySelection": false,
        "validationPolicy": "bookmarkManagement"
    }

"validationPolicy" can be one of "audioReceiver", "bookmarkManagement", "cloudUser",
"layoutAccess", "userWithEmail" or empty.

# Text

Type `text`. Represents simple text string. Does not have `properties`.

# Text with fields

Type `textWithFields`. Represents text field with templates. When the contents of this field is
processed, evaluation of template values is performed. `properties` has the following format:
    {
        "text": "{event.description}"
    }

where value of "text" property is the default value of the field.

# Time field

Type `time`. Represents duration in seconds. `properties` has the following format:

    {
        "min": 0
        "max": 60
    }

"min" is a minimal possible value, "max" is a maximal possible value.
