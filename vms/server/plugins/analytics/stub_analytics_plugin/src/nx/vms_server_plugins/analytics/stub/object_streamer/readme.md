# Stub Object Streamer Plugin

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

---------------------------------------------------------------------------------------------------

This Plugin is intended to be used for debugging/investigating/developing purposes and can generate
any needed sequence of Objects that are read from a specific user-generated file (which can be
assigned via DeviceAgent settings GUI), the format of which is described below.

## Stub Object Streamer stream file format

A stream file is a simple JSON file with a JSON array at the top level. Each element of this array is 
a JSON object that represents some Object state at a certain frame. Each object must contain some 
mandatory fields and may have some optional fields.

Mandatory fields:
- typeId (string) - the id of the corresponding Object Type.
- trackId (string, guid) - the id of the Object Track.
- frameNumber (integer) - the number of the frame the Object should be drawn on.
- boundingBox (JSON object).
    - x (float, [0, 1]) - horizontal coordinate of the top-left corner.
    - y (float, [0, 1]) - vertical coordinate of the top-left corner.
    - width (float, [0, 1]) - bounding box width.
    - height (float, [0, 1]) - bounding box height.

Optional fields:
- attributes (JSON object) - a set of the Object Attributes, keys are Attribute names, values are 
    Attribute values (only strings are allowed).
- timestampUs (integer) - timestamp of the Metadata Packet the Object will be packed into. If absent,
    the timestamp of the frame is used.

Example:

```
[
    {
        "typeId": "some.type.id",
        "trackId": "{1819c1bb-84d6-4ba5-9975-6e7e142db893}",
        "frameNumber": 0,
        "attributes":
        {
            "attribute 1": "value of attribute 1",
            "attribute 2": "1.2",
            "attribute 3": "true"
        },
        "boundingBox":
        {
            "x": 0.2,
            "y": 0.3,
            "width": 0.1,
            "height": 0.5
        },
        "timestampUs": 1628703729000000
    },
    {
        "typeId": "some.type.id",
        "trackId": "{1819c1bb-84d6-4ba5-9975-6e7e142db893}",
        "frameNumber": 1,
        "attributes":
        {
            "attribute 1": "other value of attribute 1",
            "attribute 2": "2.2",
            "attribute 3": "false"
        },
        "boundingBox":
        {
            "x": 0.22,
            "y": 0.33,
            "width": 0.11,
            "height": 0.55
        }
    }
]
```    
