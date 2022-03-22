# Stub Object Streamer stream generation script

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

The `generate.py` script is intended to be used for generation of custom stream and manifest files
for the Object Streamer sub-plugin of the Stub Analytics Plugin.

## Command line options

`--config, -c` **[required]** Path to the configuration file containing parameters of the stream to
be generated.

`--manifest-file, -m` **[optional]** Path to the output Device Agent Manifest file, default is
`manifest.json`.

`--stream-file, -s` **[optional]** Path to the output stream file, default is `stream.json`.

`--compressed-stream` **[optional]** Removes all formatting from the output stream file.

Example:
```
generate.py --config my_config.json --manifest-file my_manifest.json --stream-file stream.json
--compressed-stream
```

## Configuration file format

Configuration file is a simple JSON file with a JSON object on the top level. The following
settings are available:

`objectCount` - **[int]** Count of objects that simultaneously exist on each frame. Default is `5`.

`streamDurationInFrames` - **[int]** Duration in frames of a single stream cycle. Default is `300`.

`typeLibrary` - **[object]** Type Library section of the generated Device Agent manifest.
Auto-generatedfrom the `objectTemplates` section Object Types can be added to the `typeLibrary` of
the output manifest as well. See manifests.md for more information about Type Library and Device
Agent Manifest.

`objectTemplates` - **[array]** Settings of a single Object Track generator. The set of fields of
each template depend on generator type. Generator type is defined by the `movementPolicy` field of
the template (default and only available now value is `random`).

### Random movement generator settings

All fields are optional.

`objectTypeId` - **[string]** Object Type id of the generated Object Track. If not
defined, id is auto-generated. If id is not represented in the `typeLibrary` section, the name of
the type is auto-generated from the id and the corresponding Object Type is added to the output
Device Agent Manifest.

`minWidth` - **[float(0.0, 1.0]]** Minimum width of the Object bounding box. Default is `0.05`.

`maxWidth` - **[float(0.0, 1.0]]** Maximum width of the Object bounding box. Default is `0.3`.

`minHeight` - **[float(0.0, 1.0]]** Minimum height of the Object bounding box. Default is `0.05`.

`maxHeight` - **[float(0.0, 1.0]]** Maximum height of the Object bounding box. Default is `0.3`.

`attributes` - **[object]** Key-value pairs, key is an Object attribute name, value is
an Object attribute value. All values must be represented as strings. This set of attributes is
assigned to each Object generated from the template.

Example:
```json
{
    "objectCount": 5,
    "streamDurationInFrames": 100,
    "typeLibrary":
    {
        "objectTypes":
        [
            {
                "id": "stub.objectStreamer.custom.type.1",
                "name": "Stub OS: Custom Type 1"
            }
        ]
    },
    "objectTemplates":
    [
        {
            "objectTypeId": "nx.base.Vehicle",
            "movementPolicy": "random",
            "minWidth": 0.1,
            "minHeight": 0.1,
            "maxWidth": 0.3,
            "maxHeight": 0.3,
            "attributes":
            {
                "Color": "Red",
                "Brand": "Ford",
                "Model": "F150",
                "LicensePlate.Number": "LHD 8448",
                "LicensePlate.State/Province": "Texas",
                "LicensePlate.Country": "USA"
            }
        },
        {
            "objectTypeId": "stub.objectStreamer.custom.type.1",
            "movementPolicy": "random",
            "attributes":
            {
                "string attribute 1": "first string attribute",
                "string attribute 2": "second string attribute",
                "number attribute 1": "0.1",
                "number attribute 2": "300"
            }
        }
    ]
}

```
