# External APIs

WARNING: This is an API Preposition. Current version of this document may not be relevant to the
actual state of things in the server.

NOTE: Due to current `nx_fussion` limitations optional fields can not be skipped in the output.
Thus empty values (e.g. `""`, `[]`, `{}`, `null`) must be treated as non-present.

## GET /ec2/metrics/rules

The rules by which server calculates extra parameters and generates final manifest.
Default value is hard coded, but can be changed by POST.

Format:
```json
{
  "<resource_type_section>": "<parameter_map>"
}
```

Where `<parameter_map>`:
```json
{
  "<id>": "<parameter_group> or <parameter>"
}
```

Where `<parameter_group>`:
```json
{
  "group": "<parameter_map>"
}
```

Where `<parameter>`:
```json
{
  "name": "<parameter_name>", //< Optional.
  "calculate": "<function> <arguments>...", //< Optional.
  "insert": "<relation> <parameter_id>", //< Optional.
  "display": "<display>", //< Optional.
  "alarms": [
    "<alarm>"
  ]
}
```
- If `calculate` is present, this entry is a new parameter, and should be calculated:
  - function `const <value>` = hard coded constant
  - function `add <parameter1> <parameter2>` = parameter1 + parameter2
  - function `sub <parameter1> <parameter2>` = parameter1 + parameter2
  - function `count <parameter> [interval]` = counts parameter changes
  - function `sum <parameter> [interval]` = sums all values`
  - function `avg <parameter> <interval>` = average in interval

Where `<alarm>`:
```json
{
  "level": "<level>",
  "condition": "<function> <arguments>...",
  "text": "<template>",
}
```
- `level` is `warning` or `error`;
- `<function>` supports next values:
  - `eq <parameter1> <parameter2>` triggers if parameter1 == parameter2
  - `ne <parameter1> <parameter2>` triggers if parameter1 != parameter2
  - `gt <parameter1> <parameter2>` triggers if parameter1 > parameter2
  - `ge <parameter1> <parameter2>` triggers if parameter1 >= parameter2
  - `lt <parameter1> <parameter2>` triggers if parameter1 < parameter2
  - `le <parameter1> <parameter2>` triggers if parameter1 <= parameter2
- `<template>` is a string with keys like `{parameter}` to be substituted by parameter values.
  By default it is `{parameter.name} is {parameter}.value`

Example:
```json
{
  "system": {
    "recommendedServers": { "calculate": "const 100" }
    "servers": { "alarms": [{
      "level": "warning",
      "condition": "gt servers recommendedServers",
      "text": "The maximum number of {recommendedServers} servers per system is reached. Create another system to use more servers"
    }]},
    "recommendedCameraChannels": { "calculate": "const 1000" }
    "cameraChannels": { "alarms": [{
      "level": "warning",
      "condition": "gt cameraChannels recommendedCameraChannels",
      "text": "The maximum number of {recommendedCameraChannels} cameras per system is reached. Create another system to use more cameras"
    }]},
    "licenses": { "group": {
      "professional": { "alarms": [{
        "level": "danger",
        "condition": "gt requiredProfessional 0",
        "text": "{requiredProfessional} Professional License Channels are required"
      }, {
        "level": "warning",
        "condition": "gt expiringProfessional 0",
        "text": "{expiringProfessional} Professional License Channels expiring within ??? days"
      }]},
      ...
    }},
    ...
  },
  "servers": {
    "availability": { "group": {
      "status": { "alarms": [{
        "level": "error",
        "condition": "ne status 'Online'"
      }]},
      "offlineEvents": { "alarms": [{
        "level": "error",
        "condition": "gt offlineEvents 0"
      }]},
    }},
    "availability": { "group": {
      "recommendedCpuUsageP": [{ "calculate": "eq 95" }],
      "totalCpuUsageP": { "alarms": [{
        "level": "warning",
        "condition": "gt totalCpuUsageP recommendedCpuUsageP"
      }]},
      ...
    }},
    ...
  },
  "cameras": {
    "status": { "alarms": [
      { "level": "warning", "condition": "eq status 'Unauthorized'" },
      { "level": "danger", "condition": "eq status 'Offline'" }
    ]},
    "issues": { "group": {
      "offlineEvents": { "alarms": [{ "level": "warning", "condition": "gt offlineEvents 0" }] },
      "streamIssues": { "alarms": [{ "level": "warning", "condition": "gt streamIssues 0" }] },
      "ipConflicts": { "alarms": [{ "level": "warning", "condition": "gt ipConflicts 0" }] }
    }},
    "primaryStream": { "group": {
      "bitrate": { "alarms": [{ "level": "error", "condition": "eq bitrate 0" }] },
      "actualFps": { "alarms": [{ "level": "warning", "condition": "gt fpsDrops 0" }] }
    }},
    ...
  },
  ...
}
```

## GET /ec2/metrics/manifest

The manifest for `/ec2/metrics/value` visualization. Parameters:
- noRules (optional) - do not include parameters from rules if present;
- compact - do not include parameters with display none.

Format:
```json
{
  "<resource_type_section>": "<parameter_list>"
}
```

Where `<parameter_list>`:
```json
[
  "<parameter_group> or <parameter>"
]
```

Where `<parameter_group>`:
```json
{
  "id": "id>",
  "name": "<name>", //< Optional.
  "group": "<parameter_list>"
}
```
- If `name` is not present is is the same as `id`.

Where `<parameter>`:
```json
{
  "id": "id>",
  "name": "<name>", //< Optional.
  "unit": "<unit>", // Optional.
  "display": "<display>", // Optional.
}
```
- If `name` is not present is is the same as `id`.
- If `unit` is present, it should be shown next to the value in all times.
- If `<display>`
  - `none` - parameter should not be visible to the user;
  - `table` - parameter is visible in the table only;
  - `panel` - parameter is visible in the right panel only;
  - `table&panel` - parameter is visible in both the table and the panel.

Example:
```json
{
  "systems": [
    { "id": "servers", "name": "Servers", "display": "panel" },
    { "id": "offlineServers", "name": "Offline Servers", "display": "panel" },
    { "id": "users", "name": "Users", "display": "panel" },
    { "id": "cameraChannels", "name": "Camera Channels", "display": "panel" },
    {
      "id": "licenses",
      "group": [
        { "id": "professional", "display": "panel" },
        { "id": "requiredProfessional", "display": "" }, //< May be omitted by compact option.
        { "id": "expiringProfessional", "display": "" }, //< May be omitted by compact option.
        ...
      ]
    },
    { "id": "recommendedServers", "display": "" }, //< May be omitted by compact option.
    { "id": "recommendedCameraChannels", "display": "" }, //< May be omitted by compact option.
  ],
  "servers": [
    {
      "id": "availability",
      "group": [
        { "id": "status", "name": "Status", "display": "table&panel" },
        { "id": "offlineEvents", "name": "Offline Events", "display": "table&panel" },
        { "id": "uptime", "name": "Uptime", "unit": "s", "display": "table&panel" },
      ],
    }, {
      "id": "load",
      "group": [
        { "id": "totalCpuUsageP", "name": "CPU Usage", "unit": "%", "display": "table&panel" },
        { "id": "serverCpuUsageP", "name": "CPU Usage (VMS Server)", "unit": "%", "display": "table&panel" },
        { "id": "ramUsageB", "name": "RAM Usage", "unit": "b", "display": "panel" },
        { "id": "ramUsageP", "name": "RAM Usage", "unit": "%", "display": "table&panel" },
        ...
        { "id": "recommendedCpuUsageP", "unit": "%", "display": ""}, //< May be omitted by compact option.
        ...
      ]
    },
    ...
    }
  ],
  "cameras": [
    { "id": "type", "name": "IP", "display": "table&panel" },
    { "id": "ip", "name": "IP", "display": "table&panel" },
    { "id": "status", "name": "Status", "display": "table&panel" },
    ...
    {
      "id": "issues",
      "group": [
        { "id": "offlineEvents", "name": "Offline Events", "display": "table&panel" },
        { "id": "streamIssues", "name": "Stream Issues (1h)", "display": "table&panel" },
        { "id": "ipConflicts", "name": "IP conflicts (3m)", "display": "panel" }
      ]
    },
    {
      "id": "primaryStream",
      "name": "primary stream",
      "group": [
        { "id": "targetFps", "name": "Target FPS", "unit": "fps", "display": "table&panel" },
        { "id": "actualFps", "name": "Actual FPS", "unit": "fps", "display": "table&panel" },
        { "id": "fpsDrops", "unit": "fps", "display": "" }, //< May be omitted by compact option.
        ...
      ]
    }
    ...
  ]
}
```

## GET /ec2/metrics/values

Current state of the values (including values calculated by server). Initially returns JSON values, in
future will support WebSocket for push notifications and time period queries. Parameters:
- noRules (optional) - do not include values from rules if present;
- compact - do not include parameters with display none;
- timeline (optional) - return timestamp-value map instead of just values if present. Rules do not
  apply in this case.

Format:
```json
{
  "<resource_type_section>": {
    "<resource_uuid>": {
      "name": "<resource_name>",
      "parent": "<resource_uuid>",
      "values": "<parameter_value_group>"
    }
  }
}
```

Where `<parameter_value_group>`:
```json
{
  "<parameter_id>": "<parameter_value> or <parameter_value_group>"
}
```

Example:
```json
{
  "systems": {
    "SYSTEM_UUID_1" : {
      "name": "System 1",
      "values": {
        "servers": 105,
        "offlineServers": 0,
        "users": 10,
        "cameraChannels": 100,
        "licenses": {
          "professional": 20,
          "requiredProfessional": 5, //< May be omitted by compact option.
          "expiringProfessional": 10, //< May be omitted by compact option.
          ...
        },
        ...
        "recommendedServers": 100, //< May be omitted by compact option.
        "recommendedCameraChannels": 1000, //< May be omitted by compact option.
      }
    }
  }
  "servers": {
    "SERVER_UUID_1": {
      "name": "Server 1",
      "parent": "SYSTEM_UUID_1",
      "values": {
        "availability": { "status": "Online", "offlineEvents": 0, "uptime": 111111 },
        "load": {
          "totalCpuUsageP": 95,
          "serverCpuUsageP": 90,
          "ramUsageB": 2222222222,
          "ramUsageP": 55,
          "recommendedCpuUsageP": 90, //< May be omitted by compact option.
          ...
        },
        ...
      }
    },
    "SERVER_UUID_2": {
      "name": "Server 2",
      "parent": "SYSTEM_UUID_1",
      "values": {
        "availability": { "status": "Offline" }
        // Most of the values are not present because we can not pull them from the offline server.
      }
    },
    ...
  },
  "cameras": {
    "CAMERA_UUID_1": {
      "name": "DWC-112233",
      "parent": "SERVER_UUID_1",
      "values": {
        "type": "camera",
        "ip": "192.168.0.101",
        "status": "Online",
        "issues": { "offlineEvents": 0, "streamIssues": 0, "ipConflicts": 0 },
        "primaryStream": { "bitrate": 2555555, "targetFps": 30, "actualFps": 25, "fpsDrops": 5 },
        ...
      }
    },
    "CAMERA_UUID_2": {
      "name": "ACTi-456",
      "parent": "SERVER_UUID_1",
      "values": {
        "type": "camera",
        "ip": "192.168.0.102",
        "status": "Unauthorized",
        "issues": { "offlineEvents": 2, "streamIssues": 0, "ipConflicts": 0 },
        "primaryStream": { "bitrate": 2555555, "targetFps": 30, "actualFps": 25, "fpsDrops": 5 },
        ...
      }
    },
    ...
  }
}
```

## GET /ec2/metrics/alarms

Current state of the alarms by data from the rules.

Format:
```json
[
  "<alarm>"
]
```

Where `<alarm>`:
```json
{
  "resource": "<resource_uuid>",
  "parameter": "<dot_separated_parameter_path>",
  "level": "warning|danger",
  "text": "<substituted template to display",
}
```

Example:
```json
[
  {
    "resource": "SYSTEM_UUID_1",
    "parameter": "servers",
    "level": "warning",
    "text": "The maximum number of 100 servers per system is reached. Create another system to use more servers",
  }, {
    "resource": "SYSTEM_UUID_1",
    "parameter": "licenses.professional",
    "level": "danger",
    "text": "5 Professional License Channels are required",
  }, {
    "resource": "SYSTEM_UUID_1",
    "parameter": "licenses.professional",
    "level": "warning",
    "text": "10 Professional License Channels expiring within ??? days",
  }, {
    "resource": "SERVER_UUID_1",
    "parameter": "load.totalCpuUsageP",
    "level": "danger",
    "text": "CPU Usage is 95%",
  }, {
    "resource": "SERVER_UUID_2",
    "parameter": "availability.status",
    "level": "warning",
    "text": "Status is Offline",
  }, {
    "resource": "CAMERA_UUID_1",
    "parameter": "primaryStream.targetFps",
    "level": "warning",
    "text": "Actual FPS is 30",
  }, {
    "resource": "CAMERA_UUID_2",
    "parameter": "status",
    "level": "warning",
    "text": "Status is Unauthorized",
  }, {
    "resource": "CAMERA_UUID_2",
    "parameter": "issues.offlineEvents",
    "level": "warning",
    "text": "Offline Events is 2",
  },
  ...
]
```

# Internal APIs

## GET /api/metrics/value

The same as `/ec2/metrics/values` but returns values from single server only, used in it's
implementation, format is the same.

