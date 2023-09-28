# External APIs

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

WARNING: This is an API Proposal. Current version of this document may not be relevant to the
actual state of things in the Server.

## GET /ec2/metrics/rules

The rules by which server calculates extra parameters and generates final manifest.
Default value is hard coded, but can be changed by POST.

Format:
```json
{
  "<resourceGroupId>": { // e.g. systems / servers/ cameras / etc.
    "name": "<resourceGroupName>", // e.g. System / Servers / Cameras / etc. Default is capitalized resourceGroupId.
    "resource": "<resourceLabel>", // e.g. Server / Camera / etc.
    "values": [
      "<groupId>": {
        "name": "<groupName>",
        "values": {
          "<parameterId>": {
            "name": "<parameterName>", //< Optional: overrides parameter name.
            "description": "<parameterDescription>", // Optional: overrides description.
            "display": "<display>", //< Optional: overrides parameter display.
            "calculate": "<function> <arguments>...", //< Required for new parameters: calculates value.
            "insert": "<relation> <parameterId>", //< Optional for new parameters: specifies value.
            "alarms": [ // Optional: specifies alarm conditions for this parameter.
              {
                "level": "warning|error",
                "condition": "<comparator> <arguments>...",
                "text": "<template>", //< Optional, by default `<parameter.name> is <parameter.value>`.
              }
            ]
          }
        }
      }
    ]
  }
}
```
This data is for internal use only, detailed documentation can only be found in server source code.

Example:
```json
{
  "systems": {
    "name": "System",
    "values": {
      "info": {
        "recommendedServers": { "calculate": "const 100" },
        "servers": { "alarms": [{
          "level": "warning",
          "condition": "greaterThan %servers %recommendedServers",
          "text": "The maximum number of %recommendedServers servers per system is reached. Create another system to use more servers"
        }]},
        "recommendedCameraChannels": { "calculate": "const 1000" },
        "cameraChannels": { "alarms": [{
          "level": "warning",
          "condition": "greaterThan %cameraChannels %recommendedCameraChannels",
          "text": "The maximum number of %recommendedCameraChannels cameras per system is reached. Create another system to use more cameras"
        }]}
      },
      "licenses": {
        "professional": { "alarms": [
          {
            "level": "error",
            "condition": "greaterThan %requiredProfessional 0",
            "text": "%requiredProfessional Professional License Channels are required"
          }, {
            "level": "warning",
            "condition": "greaterThan %expiringProfessional 0",
            "text": "expiringProfessional Professional License Channels expiring within 30 days"
          }
        ]},
        ...
      }
    }
  },
  "servers": {
    "name": "Servers",
    "resource": "Server",
    "values": {
      "availability": {
        "status": { "alarms": [{ "level": "error", "condition": "notEqual %status Online" }]},
        "offlineEvents": { "alarms": [{ "level": "warning", "condition": "greaterThan %offlineEvents 0" }]},
      },
      "load": {
        "recommendedCpuUsageP": { "calculate": "const 95" },
        "totalCpuUsageP": { "alarms": [{
          "level": "warning",
          "condition": "greaterThan %totalCpuUsageP %recommendedCpuUsageP",
          "text": "CPU usage is %totalCpuUsageP"
        }]},
        ...
      },
      ...
    }
  },
  "cameras": {
    "name": "Cameras",
    "resource": "Server",
    "values": {
      "info": {
        "status": { "alarms": [
          { "level": "warning", "condition": "equal status 'Unauthorized'" },
          { "level": "error", "condition": "equal status 'Offline'" }
        ]}
      },
      "issues": { "group": {
        "offlineEvents": { "alarms": [{ "level": "warning", "condition": "greaterThan offlineEvents 0" }] },
        "streamIssues": { "alarms": [{ "level": "warning", "condition": "greaterThan streamIssues 0" }] },
        "ipConflicts": { "alarms": [{ "level": "warning", "condition": "greaterThan ipConflicts 0" }] }
      }},
      "primaryStream": { "group": {
        "bitrate": { "alarms": [{ "level": "error", "condition": "equal bitrate 0" }] },
        "actualFps": { "alarms": [{ "level": "warning", "condition": "greaterThan fpsDrops 0" }] }
      }},
      ...
    }
  },
  ...
}
```

## GET /ec2/metrics/manifest

The manifest for `/ec2/metrics/value` visualization.

Format:
```json
[
  {
    "id": "<resourceGroupId>",
    "name": "<resourceGroupName>", //< Should be shown at tab button.
    "resource": "<resourceLabel>", //< Should be shown in alarms (with resource name) if present.
    "values": [
      {
        "id": "<groupId>",
        "name": "<groupName>",
        "values": [
          {
            "id": "<parameterId>",
            "name": "<parameterName>",
            "description": "<parameterDescription>", //< Optional, should be shown in table instead of name if specified.
            "format": "<format>", //< Optional, should be bound to parameter name when shown to the user.
            "display": "<display>", //< Optional, specifies where this values should be shown to the user.
          }
        ]
      }
    ]
  }
]
```
- Supported `<display>` values:
  - `none` - parameter should not be visible to the user;
  - `table` - parameter is visible in the table only;
  - `panel` - parameter is visible in the right panel only;
  - `table&panel` - parameter is visible in both the table and the panel.
- Supported `<format>` rules are automatically applied in alarms only, the rules can be found in:
  `open/vms/libs/nx_vms_api/src/nx/vms/api/metrics.cpp` function `makeFormatter`.

Example:
```json
[
  {
    "id": "systems",
    "name": "System",
    "values": [
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
    ]
  }, {
    "id": "servers",
    "name": "Servers",
    "resource": "Server",
    "values": [
      {
        "id": "availability",
        "name": "Availability",
        "values": [
          { "id": "name", "name": "Name", "display": "table|panel" },
          { "id": "status", "name": "Status", "display": "table|panel" },
          { "id": "offlineEvents", "name": "Offline Events", "display": "table|panel" },
          { "id": "uptimeS", "name": "Uptime", "format": "duration", "display": "table|panel" },
          ...
        ],
      }, {
        "id": "load",
        "name": "Load",
        "values": [
          { "id": "totalCpuUsageP", "name": "CPU Usage", "format": "%", "display": "table|panel" },
          { "id": "serverCpuUsageP", "name": "CPU Usage (VMS Server)", "format": "%", "display": "table|panel" },
          { "id": "recommendedCpuUsageP", "format": "%", "display": ""}, //< May be omitted by compact option.
          { "id": "ramUsageB", "name": "RAM Usage", "format": "GB", "display": "panel" },
          { "id": "ramUsageP", "name": "RAM Usage", "format": "%", "display": "table|panel" },
          ...
        ]
      },
      ...
    ],
  }, {
    "id": "cameras",
    "name": "Cameras",
    "resource": "Camera",
    "values": [
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
        ]
      }, {
        "id": "issues",
        "name": "Issues",
        "values": [
          { "id": "offlineEvents", "name": "Offline Events", "display": "table|panel" },
          { "id": "streamIssues", "name": "Issues (1h)", "description": "Stream Issues (1h)", "display": "table|panel" },
          { "id": "ipConflicts", "name": "IP conflicts (3m)", "display": "panel" }
          ...
        ]
      }, {
        "id": "primaryStream",
        "name": "Primary Stream",
        "values": [
          { "id": "targetFps", "name": "Target FPS", "format": "fps", "display": "table|panel" },
          { "id": "actualFps", "name": "Actual FPS", "format": "fps", "display": "table|panel" },
          { "id": "fpsDrops", "format": "fps", "display": "" }, //< May be omitted by compact option.
          ...
        ]
      }
      ...
    ]
  }
]
```

## GET /ec2/metrics/values

Current state of the values (including values calculated by server). Optional parameters:
- `?formatted` - apply format (like in alarms). Should be used for debug only.

Format:
```json
{
  "<resourceGroupId>": {
    "<resourceId>": {
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
    "SYSTEM_UUID_1": {
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
{
  "<resourceGroupId>": {
    "<resourceId>": {
      "<groupId>": {
        "<parameterId>": [
          {
            "level": "warning|error",
            "text": "<alarmText>"
          }
        ]
      }
    }
  }
}
```
* <alarmText> Should be shown next to a resource value in table and panel as is
* <alarmText> Should be shown on Alarms tab with preprocessing:
  prefixed by <resourceLabel> (from manifest) and resource name (from values) if <resourceLabel> is not empty.

Example:
```json
{
  "systems": {
    "SYSTEM_UUID_1": {
      "info": {
        "servers": [
          {
            "level": "warning",
            "text": "The maximum number of 100 servers per system is reached. Create another system to use more servers"
          }
        ]
      },
      "licenses": {
        "professional": [
          { "level": "error", "text": "5 Professional License Channels are required" },
          { "level": "error", "text": "10 Professional License Channels expiring within 30 days" }
        ]
      }
    }
  },
  "servers": {
    "SERVER_UUID_1": { "load": { "totalCpuUsageP": [ { "level": "warning", "text": "CPU Usage is 95%" } ] } },
    "SERVER_UUID_2": { "availability": { "status": [ { "level": "warning", "text": "Status is Offline" } ] } }
  },
  "servers": {
    "CAMERA_UUID_1": {
      "primaryStream": { "targetFps": [ { "level": "warning", "text": "Actual FPS is 30" } ] }
      "issues": { "offlineEvents": [ { "level": "warning", "text": "Offline Events is 2" } ] }
    },
    "CAMERA_UUID_2": {
      "info": { "status": [ { "level": "warning", "text": "Status is Unauthorized" } ] }
    }
  }
}
```

## GET /ec2/metrics/report (TODO)

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

## GET /api/metrics/{values|alarms}

Return the same result as `/ec2/metrics/{values|alarms}` but includes only resources and values from target
server only. Used internally to implement ec2 versions. Optional parameters:
- `?formatted` - apply format (like in alarms). Should be used for debug only;
- `&system` - also show all of the data known to this server (system values for system resources).
