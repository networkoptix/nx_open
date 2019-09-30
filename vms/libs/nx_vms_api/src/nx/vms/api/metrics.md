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
  "<resourceTypeGroup>": [ // e.g. systems / servers/ cameras / etc.
    {
      "<groupId>": {
        "<parameterId>": {
          "name": "<parameter_name>", //< Optional: overrides parameter name.
          "display": "<display>", //< Optional: overrides parameter display.
          "calculate": "<function> <arguments>...", //< Required for new parameters: calculates value.
          "insert": "<relation> <parameterId>", //< Optional for new parameters: specifies value.
          "alarms": [ // Optional: specifies alarm conditions for this parameter.
            {
              "level": "warning|danger",
              "condition": "<comparator> <arguments>...",
              "text": "<template>", //< Optional, by default `<parameter.name> is <parameter.value>`.
            }
          ]
        }
      }
    }
  ],
}
```
- Supported `<function>` variants:
  - `const <value>` - hard coded constant;
  - `add <parameter1> <parameter2>` - parameter1 + parameter2;
  - `sub <parameter1> <parameter2>` - parameter1 + parameter2;
  - `count <parameter> [interval]` - counts parameter changes;
  - `sum <parameter> [interval]` - sums all values`;
  - `avg <parameter> <interval>` - average in interval.
- Supported `<comparator>` variants:
  - `eq <parameter1> <parameter2>` - triggers if parameter1 == parameter2;
  - `ne <parameter1> <parameter2>` - triggers if parameter1 != parameter2;
  - `gt <parameter1> <parameter2>` - triggers if parameter1 > parameter2;
  - `ge <parameter1> <parameter2>` - triggers if parameter1 >= parameter2;
  - `lt <parameter1> <parameter2>` - triggers if parameter1 < parameter2;
  - `le <parameter1> <parameter2>` - triggers if parameter1 <= parameter2.
- `<template>` is a string with keys like `{parameter}` to be substituted by parameter values.

Example:
```json
{
  "system": {
    "info": {
      "recommendedServers": { "calculate": "const 100" },
      "servers": { "alarms": [{
        "level": "warning",
        "condition": "gt %servers %recommendedServers",
        "text": "The maximum number of %recommendedServers servers per system is reached. Create another system to use more servers"
      }]},
      "recommendedCameraChannels": { "calculate": "const 1000" },
      "cameraChannels": { "alarms": [{
        "level": "warning",
        "condition": "gt %cameraChannels %recommendedCameraChannels",
        "text": "The maximum number of %recommendedCameraChannels cameras per system is reached. Create another system to use more cameras"
      }]}
    },
    "licenses": {
      "professional": { "alarms": [
        {
          "level": "danger",
          "condition": "gt %requiredProfessional 0",
          "text": "%requiredProfessional Professional License Channels are required"
        }, {
          "level": "warning",
          "condition": "gt %expiringProfessional 0",
          "text": "expiringProfessional Professional License Channels expiring within 30 days"
        }
      ]},
      ...
    }
  },
  "servers": {
    "availability": {
      "status": { "alarms": [{ "level": "danger", "condition": "ne %status Online" }]},
      "offlineEvents": { "alarms": [{ "level": "warning", "condition": "gt %offlineEvents 0" }]},
    },
    "load": {
      "recommendedCpuUsageP": { "calculate": "const 95" },
      "totalCpuUsageP": { "alarms": [{
        "level": "warning",
        "condition": "gt %totalCpuUsageP %recommendedCpuUsageP",
        "text": "CPU usage is %totalCpuUsageP"
      }]},
      ...
    },
    ...
  },
  "cameras": {
    "info": {
      "status": { "alarms": [
        { "level": "warning", "condition": "eq status 'Unauthorized'" },
        { "level": "danger", "condition": "eq status 'Offline'" }
      ]}
    },
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
- compact (optional) - do not include parameters with display none.

Format:
```json
{
  "<resourceTypeGroup>": [ // e.g. systems / servers/ cameras / etc.
    {
      "id": "<groupId>",
      "name": "<groupName>",
      "values": [
        {
          "id": "<parameterId>",
          "name": "<parameterName>",
          "unit": "<unit>", //< Optional, should be bound to parameter name when shown to the user.
          "display": "<display>", //< Optional, specifies where this values should be shown to the user.
        }
      ]
    }
  ]
}
```
- Supported `<display>` values:
  - `none` - parameter should not be visible to the user;
  - `table` - parameter is visible in the table only;
  - `panel` - parameter is visible in the right panel only;
  - `table&panel` - parameter is visible in both the table and the panel.

Example:
```json
{
  "systems": [
    {
      "id": "info",
      "name": "Info",
      "values": [
        { "id": "name", "name": "Name", "display": "panel" },
        { "id": "servers", "name": "Servers", "display": "panel" },
        { "id": "offlineServers", "name": "Offline Servers", "display": "panel" },
        { "id": "users", "name": "Users", "display": "panel" },
        { "id": "cameraChannels", "name": "Camera Channels", "display": "panel" },
        { "id": "recommendedServers", "display": "" }, //< May be omitted by compact option.
        { "id": "recommendedCameraChannels", "display": "" }, //< May be omitted by compact option.
      ],
    }, {
      "id": "licenses",
      "name": "Licenses",
      "values": [
        { "id": "professional", "display": "panel" },
        { "id": "requiredProfessional", "display": "" }, //< May be omitted by compact option.
        { "id": "expiringProfessional", "display": "" }, //< May be omitted by compact option.
        ...
      ]
    }
  ],
  "servers": [
    {
      "id": "availability",
      "name": "Availability",
      "values": [
        { "id": "name", "name": "Name", "display": "table|panel" },
        { "id": "status", "name": "Status", "display": "table|panel" },
        { "id": "offlineEvents", "name": "Offline Events", "display": "table|panel" },
        { "id": "uptime", "name": "Uptime", "unit": "s", "display": "table|panel" },
        ...
      ],
    }, {
      "id": "load",
      "name": "Load",
      "values": [
        { "id": "totalCpuUsageP", "name": "CPU Usage", "unit": "%", "display": "table|panel" },
        { "id": "serverCpuUsageP", "name": "CPU Usage (VMS Server)", "unit": "%", "display": "table|panel" },
        { "id": "recommendedCpuUsageP", "unit": "%", "display": ""}, //< May be omitted by compact option.
        { "id": "ramUsageB", "name": "RAM Usage", "unit": "b", "display": "panel" },
        { "id": "ramUsageP", "name": "RAM Usage", "unit": "%", "display": "table|panel" },
        ...
      ]
    },
    ...
  ],
  "cameras": [
    {
      "id": "info",
      "name": "Info",
      "values": [
        { "id": "name", "name": "Name", "display": "table|panel" },
        { "id": "server", "name": "Server", "display": "table|panel" },
        { "id": "type", "name": "Type", "display": "table|panel" },
        { "id": "ip", "name": "IP", "display": "table|panel" },
        { "id": "status", "name": "Status", "display": "table|panel" },
      ...
    }, {
      "id": "issues",
      "name": "Issues",
      "values": [
        { "id": "offlineEvents", "name": "Offline Events", "display": "table|panel" },
        { "id": "streamIssues", "name": "Stream Issues (1h)", "display": "table|panel" },
        { "id": "ipConflicts", "name": "IP conflicts (3m)", "display": "panel" }
        ...
      ]
    }, {
      "id": "primaryStream",
      "name": "Primary Stream",
      "values": [
        { "id": "targetFps", "name": "Target FPS", "unit": "fps", "display": "table|panel" },
        { "id": "actualFps", "name": "Actual FPS", "unit": "fps", "display": "table|panel" },
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
- compact (optional) - do not include parameters with display none.

Format:
```json
{
  "<resource_type_section>": {
    "<resource_uuid>": {
      "<groupId>": {
        "<parameterId>": "<parameterValue>"
      }
    }
  }
}
```

Example:
```json
{
  "systems": {
    "SYSTEM_UUID_1" : {
      "info": {
        "name": "System 1",
        "servers": 105,
        "offlineServers": 0,
        "users": 10,
        "cameraChannels": 100,
        ...
        "recommendedServers": 100, //< May be omitted by compact option.
        "recommendedCameraChannels": 1000, //< May be omitted by compact option.
      },
      "licenses": {
        "professional": 20,
        "requiredProfessional": 5, //< May be omitted by compact option.
        "expiringProfessional": 10, //< May be omitted by compact option.
        ...
      },
      ...
    }
  },
  "servers": {
    "SERVER_UUID_1": {
      "availability": { "name": "Server 1", "status": "Online", "offlineEvents": 0, "uptime": 111111 },
      "load": {
        "totalCpuUsageP": 95,
        "serverCpuUsageP": 90,
        "ramUsageB": 2222222222,
        "ramUsageP": 55,
        "recommendedCpuUsageP": 90, //< May be omitted by compact option.
        ...
      },
      ...
    },
    "SERVER_UUID_2": {
      "availability": { "name": "Server 2", "status": "Offline" }
      // Most of the values are not present because we can not pull them from the offline server.
    },
    ...
  },
  "cameras": {
    "CAMERA_UUID_1": {
      "info": {
        "name": "DWC-112233", "server": "Server 1",
        "type": "camera", "ip": "192.168.0.101", "status": "Online", ...
      },
      "issues": { "offlineEvents": 0, "streamIssues": 0, "ipConflicts": 0, ... },
      "primaryStream": { "bitrate": 2555555, "targetFps": 30, "actualFps": 25, "fpsDrops": 5, ... },
      ...
    },
    "CAMERA_UUID_2": {
      "info": {
        "name": "ACTi-456", "server": "Server 1",
        "type": "camera", "ip": "192.168.0.102", "status": "Unauthorized", ...
      },
      "issues": { "offlineEvents": 2, "streamIssues": 0, "ipConflicts": 0, ... },
      "primaryStream": { "bitrate": 1333333, "targetFps": 15, "actualFps": 15, "fpsDrops": 5, ...},
      ...
    },
    "CAMERA_UUID_3": {
      "info": {
        "name": "iqEve-666", "server": "Server 2",
        "type": "camera", "ip": "192.168.0.103", "status": "Offline", ...
      },
      ...
    },
    ...
  },
  ...
}
```

## GET /ec2/metrics/alarms

Current state of the alarms by data from the rules.

Format:
```json
[
  {
    "resource": "<resourceId>",
    "parameter": "<groupId>.<parameterId>",
    "level": "warning|danger",
    "text": "<displayText>",
  }
]
```

Example:
```json
[
  {
    "resource": "SYSTEM_UUID_1",
    "parameter": "servers",
    "level": "warning",
    "text": "The maximum number of 100 servers per system is reached. Create another system to use more servers"
  }, {
    "resource": "SYSTEM_UUID_1",
    "parameter": "licenses.professional",
    "level": "danger",
    "text": "5 Professional License Channels are required"
  }, {
    "resource": "SYSTEM_UUID_1",
    "parameter": "licenses.professional",
    "level": "danger",
    "text": "10 Professional License Channels expiring within ??? days"
  }, {
    "resource": "SERVER_UUID_1",
    "parameter": "load.totalCpuUsageP",
    "level": "warning",
    "text": "CPU Usage is 95%"
  }, {
    "resource": "SERVER_UUID_2",
    "parameter": "availability.status",
    "level": "warning",
    "text": "Status is Offline"
  }, {
    "resource": "CAMERA_UUID_1",
    "parameter": "primaryStream.targetFps",
    "level": "warning",
    "text": "Actual FPS is 30"
  }, {
    "resource": "CAMERA_UUID_2",
    "parameter": "status",
    "level": "warning",
    "text": "Status is Unauthorized"
  }, {
    "resource": "CAMERA_UUID_3",
    "parameter": "issues.offlineEvents",
    "level": "warning",
    "text": "Offline Events is 2"
  },
  ...
]
```

## GET /ec2/metrics/report

All information in a single request. Params:
- rules = yes|no (optional), default yes;
- manifest = yes|no (optional), default yes;
- values = yes|no (optional), default yes;
- alarms = yes|no (optional), default yes.

```
{
  "rules": "</ec/metrics/rules>",
  "manifest": "</ec/metrics/manifest>",
  "values": "</ec/metrics/values>",
  "alarms": "</ec/metrics/alarms>",
}
```

# Internal APIs

## GET /api/metrics/{rules|manifest}

Return the same result as `/ec2/metrics/{rules|manifest}`, because rules and manifest are system
wide.

## GET /api/metrics/{values|alarms}

Return the same result as `/ec2/metrics/{values|alarms}` but includes only resources from target
server only. Used internally to implement ec2 versions.

