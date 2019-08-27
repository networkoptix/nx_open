# External APIs

NOTE: Due to current `nx_fussion` limitations optional fields can not be skipped in the output.
Thus empty values (e.g. `""`, `[]`, `{}`, `null`) must be treated as non-present.

## GET /ec2/metrics/rules

The rules by which server calculates extra paramiters and renerates final manifest.
Default value is hardcoded, but can be changed by POST.

Format:
```json
{
    "<resource_type_section>": <parameter_map>
}
```

Where `<parameter_map>`:
```json
{
    "<id>": <parameter_group> or <parameter>
}
```

Where `<parameter_group>`:
```json
{
    "group": `<parameter_map>`
}
```

Where `<parameter>`:
```json
{
    "alarms": { //< Optional.
        "warning": "<alarm_value>", //< Optional.
        "error": " <alarm_value>" //< Optional.
    },
    "calculate": "<function> <arguments>...", //< Optional.
    "name": "<parameter_name>", //< Optional.
    "insert": "<relation> <parameter_id>", //< Optional.
}
```
- If `<alarm_value>`:
  - starts with `=`, `!=`, `>`, `<`, `>=`, `<=` - apply this function to whatever follows;
  - is `numeric` it's checked by `>=`;
  - otherwise it's checks by `==`.
- If `calculate` is present, this entry is a new parameter, and should be calculated:
  - function `add <parameter1> <parameter2>` = parameter1 + parameter2
  - function `sub <parameter1> <parameter2>` = parameter1 + parameter2
  - function `count <parameter> [interval]` = counts parameter changes
  - function `sum <parameter> [interval]` = sums all values`
  - function `avg <parameter> <interval>` = average in interval
- Fields `name` and `insert` may be present only if `calculate` is present as well (a new
  parameter wth calculated value will be created in manifest and values outputs).

Example:
```json
{
    "servers": {
        "status": { "alarms": { "error": "Offline" } },
        "statusChanges": {
            "name": "status changes last hour",
            "calculate": "changes status 1h",
            "insert": "after status",
            "alarms": { "warning": 1, "error": 10 }
        },
        "cpuUsage": { "alarms": { "warning": 70, "error": 90 } },
        "cpuMaxUsage": {
            "name": "max CPU usage last hour",
            "calculate": "max cpuUsage 1h",
            "insert": "after cpuUsage",
            "alarms": { "warning": 70, "error": 90 }
        },
        ...
    },
    "cameras": {
        "status": { "alarms": { "warning": "Unauthorized", "error": "Offline" } },
        "statusChanges": {
            "name": "status changes last hour",
            "calculate": "changes status 1h",
            "insert": "after status",
            "alarms": { "warning": 1, "error": 10 }
        },
        "primaryStream": {
            "group": {
                "dropFps": {
                    "name": "FPS drop",
                    "calculate": "subtract targetFps actualFps",
                    "insert": "before targetFps",
                    "alarms": { "error": 2 }
                },
                ...
            }
        },
        ...
    }
}
```

## GET /ec2/metrics/manifest

The mainest for `/ec2/metrics/value` visualization. Parameters:
- noRules (optional) - do not include parameters from rules if present.

Format:
```json
{
    "<resource_type_section>": <parameter_list>
}
```

Where `<parameter_list>`:
```json
[
    <parameter_group> or <parameter>
]
```

Where `<parameter_group>`:
```json
{
    "id": "id>",
    "name": "<name>", //< Optional.
    "group": <parameter_list>
}
```
- If `name` is not present is is the same as `id`.

Where `<parameter>`:
```json
{
    "id": "id>",
    "name": "<name>", //< Optional.
    "unit": "<unit>" // Optional.
}
```
- If `name` is not present is is the same as `id`.
- If `unit` is present, it should be shown next to the value in all times.

Example:
```json
{
    "servers": [
        { "id": "ip", "name": "IP" },
        { "id": "os", "name": "OS" },
        { "id": "status" },
        { "id": "statusChanges", "name": "status changes last hour" },
        { "id": "cpuUsage", "name": "CPU usage", "unit": "%" },
        { "id": "cpuMaxUsage", "name": "max CPU usage last hour", "unit": "%" },
        ...
    ],
    "cameras": [
        { "id": "ip", "name": "IP" },
        { "id": "status" },
        { "id": "statusChanges", "name": "status changes last hour" },
        {
            "id": "primaryStream",
            "name": "primary stream",
            "group": [
                { "id": "dropFps", "name": "FPS drop", "unit": "fps" },
                { "id": "targetFps", "name": "target FPS", "unit": "fps" },
                { "id": "actualFps", "name": "actual FPS", "unit": "fps" },
                ...
            ]
        }
        ...
    ]
}
```

## GET /ec2/metrics/values

Current state of the values (including values calculated by server). Initially returns JSON values, in
future will support WebSocket for push notifications and time period quieries. Parameters:
- noRules (optional) - do not include values from rules if present.
- timeline (optional) - return timestamp-value map instead of just values if present. Rules do not
  apply in this case.

Format:
```json
{
    "<resource_type_section>": {
        "<resource_uuid>": {
            "name": "<resource_name>",
            "parent": "<resource_uuid>",
            "values": {
                "<id>": <parameter_group> or <parameter>
            }
        }
    }
}
```

Where `<parameter_group>`:
```json
{
    "group": {
        "<id>": <parameter_group> or <parameter>
    }
}
```

Where `<parameter>`:
```json
{
    "value": "<value>",
    "status": "warning|error" //< Optional.
}
```
- If `status` is not present, it means `ok`.

Example:
```json
{
    "servers": {
        "SERVER_UUID_1": {
            "name": "Server 1",
            "parent": "SYSTEM_1",
            "values": {
                "ip": { "value": "192.168.0.1, 10.0.1.1" },
                "os": { "value": "Windows 10" },
                "status": { "value": "Online" },
                "statusChanges": { "value": 0 },
                "cpuUsage": { "value": 20 },
                "cpuMaxUsage": { "value": 25 },
                ...
            }
        },
        "SERVER_UUID_2": {
            "name": "Server 1",
            "parent": "SYSTEM_1",
            "values": {
                "ip": { "value": "192.168.0.2, 10.0.1.2" },
                "os": { "value": "Windows 14.04" },
                "status": { "value": "Online" },
                "statusChanges": { "value": 1, "status": "warning" },
                ...
            }
        },
        ...
    },
    "cameras": {
        "CAMERA_UUID_1": {
            "name": "DWC-112233",
            "parent": "SERVER_UUID_1",
            "values": {
                "status": { "value": "Online" },
                "statusChanges": { "value": 0 },
                "primaryStream": {
                    "group": {
                        "targetFps": { "value": 30 },
                        "actualFps": { "value": 25 },
                        "dropFps": { "value": 5, "status": "error" },
                        ...
                    }
                },
                ...
            }
        },
        "CAMERA_UUID_2": {
            "name": "ACTi-456",
            "parent": "SERVER_UUID_1",
            "values": {
                "status": { "value": "Unauthorized", "status": "warning" },
                "statusChanges": { "value": 0 },
                "primaryStream": {
                    "group": {
                        "targetFps": { "value": 30 },
                        "actualFps": { "value": 29 },
                        "dropFps": { "value": 1 },
                    }
                },
                ...
            }
        },
        ...
    }
}
```

# Internal APIs

## GET /api/metrics/value

The same as `/ec2/metrics/values` but returns values from single server only, used in it's
implementation, format is the same.

