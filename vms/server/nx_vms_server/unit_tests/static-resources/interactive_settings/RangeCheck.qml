import nx.mediaserver.interactive_settings 1.0

Settings
{
    readonly property var _values:
    {
        "number": 0,
        "double": 0,
        "combobox": "value1",
    }

    readonly property var _testValues:
    {
        "number": -1,
        "double": -1,
        "combobox": "value0",
    }

    SpinBox
    {
        name: "number"
        defaultValue: 42
        minValue: 0
        maxValue: 100
    }

    DoubleSpinBox
    {
        name: "double"
        defaultValue: 3.1415
        minValue: 0.0
        maxValue: 100.0
    }

    ComboBox
    {
        name: "combobox"
        defaultValue: "value1"
        range: ["value1", "value2", "value3"]
    }
}
