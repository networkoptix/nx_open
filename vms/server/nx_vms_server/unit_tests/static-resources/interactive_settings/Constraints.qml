import nx.mediaserver.interactive_settings 1.0

Settings
{
    readonly property var _impossibleValues:
    {
        "master": false,
        "slave": "two"
    }

    readonly property var _correctValues:
    {
        "master": true,
        "slave": "two"
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
