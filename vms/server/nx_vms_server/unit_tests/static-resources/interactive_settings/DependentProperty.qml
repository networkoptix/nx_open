import nx.mediaserver.interactive_settings 1.0

Settings
{
    readonly property var _serialized:
    {
        "type": "Settings",
        "items": [
            {
                "type": "CheckBox",
                "name": "master",
                "defaultValue": false,
                "value": master.value
            },
            {
                "type": "ComboBox",
                "name": "slave",
                "defaultValue": "one",
                "value": "one",
                "range": master.value ? ["one", "two"] : ["one"]
            }
        ]
    }

    CheckBox
    {
        id: master
        name: "master"
        defaultValue: false
    }

    ComboBox
    {
        name: "slave"
        defaultValue: "one"
        range: master.value ? ["one", "two"] : ["one"]
    }
}
