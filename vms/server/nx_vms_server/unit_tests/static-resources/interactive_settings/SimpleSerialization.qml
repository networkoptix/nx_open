import nx.mediaserver.interactive_settings 1.0

Settings
{
    readonly property var _serializedModel:
    {
        "type": "Settings",
        "items": [
            {
                "type": "TextField",
                "name": "text",
                "caption": "Text Field",
                "description": "A text field",
                "defaultValue": "a text",
            },
            {
                "type": "TextArea",
                "name": "textArea",
                "caption": "Text Area",
                "description": "A text area",
                "defaultValue": "a text"
            },
            {
                "type": "GroupBox",
                "caption": "Group",
                "items": [
                    {
                        "type": "SpinBox",
                        "name": "number",
                        "defaultValue": 42,
                        "minValue": 0,
                        "maxValue": 100
                    },
                    {
                        "type": "DoubleSpinBox",
                        "name": "double",
                        "defaultValue": 3.1415,
                        "minValue": 0.0,
                        "maxValue": 100.0
                    },
                    {
                        "type": "ComboBox",
                        "name": "combobox",
                        "defaultValue": "value2",
                        "range": ["value1", "value2", "value3"]
                    },
                    {
                        "type": "Row",
                        "items": [
                            {
                                "type": "Button",
                                "name": "button",
                                "caption": "Button"
                            },
                            {
                                "type": "CheckBox",
                                "name": "checkbox",
                                "defaultValue": true
                            }
                        ]
                    }
                ]
            }
        ]
    }

    readonly property var _values:
    {
        "text": "a text",
        "textArea": "a text",
        "number": 42,
        "double": 3.1415,
        "combobox": "value2",
        "checkbox": true
    }

    TextField
    {
        name: "text"
        caption: "Text Field"
        description: "A text field"
        defaultValue: "a text"
    }

    TextArea
    {
        name: "textArea"
        caption: "Text Area"
        description: "A text area"
        defaultValue: "a text"
    }

    GroupBox
    {
        caption: "Group"

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
            defaultValue: "value2"
            range: ["value1", "value2", "value3"]
        }

        Row
        {
            Button
            {
                name: "button"
                caption: "Button"
            }

            CheckBox
            {
                name: "checkbox"
                defaultValue: true
            }
        }
    }
}
