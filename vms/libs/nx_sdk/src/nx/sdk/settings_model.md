# Interactive Settings Engine

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

In various places in the SDK, e.g. in Manifests of Analytics Plugin entities, there is a
possibility to define a Settings Model - a JSON describing a hierarchical structure of settings,
each having its name, type, and default value.

The Client can offer a user a dialog with controls corresponding to such Model. When the user fills
the dialog, a set of values is formed, represented as a map of strings, where the key is the id of
the particular setting in the Model, and the value is either a string itself (for settings of
the type String), or a JSON-formatted value (for settings of other types).

All editable controls have a value of some type, which can be represented as a string. For values
of type `String`, this string represents the value itself. For other types, this string is a JSON
value of the appropriate types.

Some field names have the same meaning through all the controls and can have some limitations.
- `"type"` - Type of the setting. Must be one of the values enumerated here.
- `"name"` - Unique id of the setting. Used when representing a Setting Value.
- `"defaultValue"` - Default value of the control. Will be sent to the engine on applying changes.

The Settings Model itself is a JSON object, which field `"items"` is a JSON array containing
top-level Setting Models for individual Settings, and those Settings that support inner Settings
(like `GroupBox`) may have them.

    {
        "type": "Settings",
        "items": [
            ...
        ]
    }

Here are the descriptions of all available Setting types, with examples of their definition in the
Settings Model, and examples of their value strings.

---------------------------------------------------------------------------------------------------
## TextField

Text data field. Supports regex validation for the entered value.

[proprietary]

![](doc/images/text-field1.png)
![](doc/images/text-field2.png)

[/proprietary]

### Setting Model

    {
        "type": "TextField",
        "name": "textField1",
        "caption": "Text Field",
        "description": "A text field",
        "defaultValue": "a text",
        "validationErrorMessage": "Text must contain only digits and characters a-f, e.g. 12ab34cd.",
        "validationRegex": "^[a-f0-9]+$",
        "validationRegexFlags": "i"
    }

### Setting Value

    "textField1": "some text"

---------------------------------------------------------------------------------------------------
## SpinBox

Integer number data field. Supports minimum and maximum value limitations.

[proprietary]

![](doc/images/spin-box.png)

[/proprietary]

### Setting Model

    {
        "type": "SpinBox",
        "name": "integerField1",
        "caption": "Number Field",
        "description": "An integer number field",
        "defaultValue": 5,
        "minValue": 0,
        "maxValue": 100
    }

### Setting Value

    "integerField1": 10

---------------------------------------------------------------------------------------------------
## DoubleSpinBox

Floating point number data field. Supports minimum and maximum value limitations.

[proprietary]

![](doc/images/double-spin-box.png)

[/proprietary]

### Setting Model

    {
        "type": "DoubleSpinBox",
        "name": "floatingField1",
        "caption": "Number Field",
        "description": "A floating-point number field",
        "defaultValue": 3.141,
        "minValue": 0.0,
        "maxValue": 100.0
    }

### Setting Value

    "floatingField1": 5.500

---------------------------------------------------------------------------------------------------
## ComboBox

Text choice data field. Items in `itemCaptions` are optional, as well as `itemCaptions` itself.

[proprietary]

![](doc/images/combo-box1.png)
![](doc/images/combo-box2.png)

[/proprietary]

### Setting Model

    {
        "type": "ComboBox",
        "name": "combobox1",
        "caption": "Combo Box",
        "description": "Choose one",
        "defaultValue": "v2",
        "range": ["v1", "v2", "v3"],
        "itemCaptions": {
            "v1": "value1",
            "v2": "value2",
            "v3": "value3"
        }
    }

### Setting Value

    "combobox1": "v2"

---------------------------------------------------------------------------------------------------
## RadioButtonGroup

Alternative text choice data field. Items in `itemCaptions` are optional, as well as `itemCaptions`
itself.

[proprietary]

![](doc/images/radio-button-group.png)

[/proprietary]

### Setting Model

    {
        "type": "RadioButtonGroup",
        "name": "radiogroup1",
        "caption": "RadioButton Group",
        "description": "Choose one",
        "defaultValue": "opt2",
        "range": ["opt1", "opt2", "opt3", "opt4"],
        "itemCaptions": {
            "opt1": "option1",
            "opt2": "option2",
            "opt3": "option3",
            "opt4": "option4"
        }
    }

### Setting Value

    "radiogroup1": "opt2"

---------------------------------------------------------------------------------------------------
## CheckBox

Boolean data field.

[proprietary]

![](doc/images/check-box1.png)
![](doc/images/check-box2.png)

[/proprietary]

### Setting Model

    {
        "type": "CheckBox",
        "name": "checkbox1",
        "caption": "Check Box",
        "description": "True or False",
        "defaultValue": true
    }

### Setting Value

    "checkbox1": true

---------------------------------------------------------------------------------------------------
## CheckBoxGroup

One or several boolean options choice data field. Items in `itemCaptions` are optional, as well as
`itemCaptions` itself.

[proprietary]

![](doc/images/check-box-group.png)

[/proprietary]

### Setting Model

    {
        "type": "CheckBoxGroup",
        "name": "checkgroup1",
        "caption": "CheckBox Group",
        "description": "Choose one or several options",
        "defaultValue": ["opt2", "opt3"],
        "range": ["opt1", "opt2", "opt3", "opt4"],
        "itemCaptions": {
            "opt1": "Option #1",
            "opt2": "Option #2",
            "opt3": "Option #3",
            "opt4": "Option #4"
        }
    }

### Setting Value

    "checkgroup1": ["opt2", "opt3"]

---------------------------------------------------------------------------------------------------
## SwitchButton

Boolean data field, alternative visual representation.

[proprietary]

![](doc/images/switch-button.png)

[/proprietary]

### Setting Model

    {
        "type": "SwitchButton",
        "name": "switch1",
        "caption": "Switch Button",
        "description": "Tooltip for the switch button",
        "defaultValue": true
    }

### Setting Value

    "switch1": true

---------------------------------------------------------------------------------------------------
## PolygonFigure

Polygon figure field. Supports maximum points limitation.

[proprietary]

![](doc/images/polygon-figure1.png)
![](doc/images/polygon-figure2.png)

[/proprietary]

### Setting Model

    {
        "type": "PolygonFigure",
        "name": "polygon1",
        "caption": "Polygon",
        "description": "Some polygon",
        "minPoints": 4,
        "maxPoints": 8
    }

### Setting Value

    "polygon1": {
        "figure": {
            "color": "#ffab40",
            "points": [
                [0.138, 0.551],
                [0.775, 0.429],
                [0.748, 0.844]
            ]
        },
        "label": "User Polygon",
        "showOnCamera": true
    }

---------------------------------------------------------------------------------------------------
## BoxFigure

Box figure field.

[proprietary]

![](doc/images/box-figure.png)

[/proprietary]

### Setting Model

    {
        "type": "BoxFigure",
        "name": "box1",
        "caption": "Box",
        "description": "Some box"
    }

### Setting Value

    "box1": {
        "figure": {
            "color": "#536dfe",
            "points": [
                [0.28, 0.177],
                [0.598, 0.684]
            ]
        },
        "label": "User Box",
        "showOnCamera": false
    }

---------------------------------------------------------------------------------------------------
## LineFigure

Line or polyline figure field. Supports minimum and maximum points limitation (default value: 2 for
both min and max). Can have single crossing direction or not have any (equivalent to having both
directions).

[proprietary]

![](doc/images/line-figure.png)

[/proprietary]

### Setting Model

    {
        "type": "LineFigure",
        "name": "line1",
        "caption": "Line",
        "description": "Some line",
        "minPoints": 2,
        "maxPoints": 8,
        "allowedDirections": "one"
    }

`allowedDirections` restricts the set of allowed values for direction:
- `any` (default): `left | right | absent`
- `none`: `absent`
- `one`: `left | right`

### Setting Value

    "line1": {
        "figure": {
            "color": "#ffab40",
            "points":[
                [0.338, 0.500],
                [0.14, 0.346],
                [0.365, 0.805]
            ],
            "direction": "right"
        }
        "label": "User Line",
        "showOnCamera": true
    }

Direction options: `left | right | absent`

---------------------------------------------------------------------------------------------------
## ObjectSizeConstraints

Figure composed of two boxes, minimum and maximum. If the minimum box has width or height greater
than that of the maximum box, the figure is considered invalid.

Has neither label nor display on camera options. Therefore may have no color.

[proprietary]

[VMS-17379](https://networkoptix.atlassian.net/browse/VMS-17379)

![](doc/images/object-size-constraints.png)

[/proprietary]

### Setting Model

    {
        "type": "ObjectSizeConstraints",
        "name": "sizeConstraints1",
        "caption": "Person size constraints",
        "description": "Size range a person should fit into to be detected",
        "defaultValue": {
            "minimum": [0.1, 0.3],
            "maximum": [0.3, 0.9]
        }
    }

### Setting Value

    "sizeConstraints1": {
        "minimum": [0.1, 0.3],
        "maximum": [0.3, 0.9],
        "positions": [
            [0.2, 0.24],
            [0.12, 0.07]
        ]
    }

The `"positions"` field is a hint for the client where to position min/max boxes, and is optional.

[proprietary]

---------------------------------------------------------------------------------------------------
## Button

**NOT IMPLEMENTED**

![](doc/images/button.png)

Action button

[VMS-15786](https://networkoptix.atlassian.net/browse/VMS-15786)

### Setting Model

    {
        "type": "Button",
        "name": "button1",
        "caption": "Button 1",
        "description": "Action 1"
    }

### Setting Value

**NOT IMPLEMENTED**

[/proprietary]

---------------------------------------------------------------------------------------------------
## Structure

Structure components are used for the data visual representation only. They are not editable in any
matter, their state is not saved (even if can be modified). Most of them contain `items` array,
which can hold any other components.

Here are the descriptions of all available structure components.

---------------------------------------------------------------------------------------------------
## Settings

System root element. Must be present only once as the root, containing all other elements and all
sections.

[proprietary]

![](doc/images/settings.png)

[/proprietary]

### Setting Model

    {
        "type": "Settings",
        "items": [
            {
                "type": "GroupBox",
                "caption": "Sample GroupBox",
                "items": [
                    { "type": "CheckBox", "name": "checkbox1", "caption": "Sample CheckBox" }
                ]
            }
        ],
        "sections": [
            { "type": "Section", "name": "Sample Section" }
        ]
    }

---------------------------------------------------------------------------------------------------
## Section

Grouping tree-like element. Can contain both items and sub-sections.

Property "name" must be unique, will be used as a caption if caption is not specified.

[proprietary]

![](doc/images/section.png)

[/proprietary]

### Setting Model

    {
        "type": "Section",
        "name": "root_section",
        "caption": "Example",
        "items": [  { "type": "CheckBox", "name": "checkbox1", "caption": "Sample CheckBox 1" } ]
        "sections": [
            { "type": "Section", "name": "Nested Section" }
        ]
    },
    {
        "type": "Section",
        "caption": "ROI"
    }

---------------------------------------------------------------------------------------------------
## GroupBox

Grouping panel with a caption. Top-level groupboxes visually distinguished from the nested.

[proprietary]

![](doc/images/group-box.png)

[/proprietary]

### Setting Model

    {
        "type": "GroupBox",
        "caption": "Top-level groupbox",
        "items": [
            { "type": "CheckBox", "caption": "CheckBox", "description": "Hint" },
            { "type": "SpinBox", "caption": "SpinBox",  "description": "Hint" },
            {
                "type": "GroupBox",
                "caption": "Nested groupbox",
                "items": [
                { "type": "CheckBox", "caption": "CheckBox" },
                { "type": "TextField", "caption": "TextField", "description": "Hint" },
                {
                    "type": "GroupBox",
                    "caption": "Nested groupbox of level 2",
                    "items": [
                        { "type": "CheckBox", "caption": "CheckBox", "description": "Hint" },
                        { "type": "TextField", "caption": "TextField" }
                    ]
                }
            ]
            }
        ]
    }

---------------------------------------------------------------------------------------------------
## Separator

Horizontal separator line.

[proprietary]

![](doc/images/separator.png)

[/proprietary]

### Setting Model

    {
        "type": "Separator"
    }

---------------------------------------------------------------------------------------------------
## Repeater

Grouping element which generates N items from the given template and displaying them when needed.

[proprietary]

[VMS-17299](https://networkoptix.atlassian.net/browse/VMS-17299)

![](doc/images/repeater.png)

[/proprietary]

Repeater expands '#N' (or short form '#') string in its template to an item number. Nested
repeaters distinguish their numbering by N number in '#N'. To escape '#' sign, it must be doubled:
'##'.

When the Repeater is used with a generic grouping item (like `GroupBox`) and it should hide empty
items, then the grouping item should set the field `"filledCheckItems"`. This field should contain
a list of items which should be considered to decide whether the group is filled or empty.

### Setting Model

    {
        "type": "Repeater",
        "count": 10,
        "startIndex": 0,
        "template":
        {
            "type": "GroupBox",
            "caption": "Line #",
            "filledCheckItems": ["roi.figures.line#.figure"],
            "items":
            [
                {
                    "type": "LineFigure",
                    "name": "roi.figures.line#.figure"
                },
                {
                    "type": "SpinBox",
                    "name": "roi.figures.line#.sensitivity",
                    "caption": "Sensitivity"
                },
                {
                    "type": "Repeater",
                    "count": 5,
                    "startIndex": 1,
                    "template":
                    {
                        "type": "TextField",
                        "name": "roi.figures.line#1.comment#2",
                        "caption": "Comment ###"
                    }
                }
            ]
        }
    }

[proprietary]

---------------------------------------------------------------------------------------------------
## Row

**INTERNAL**

Re-ordering element to place items horizontally in a single row. Can lead to strange visual
effects, so keeping it for internal usage.

![](doc/images/row.png)

### Setting Model

    {
        "type": "Row",
        "items": [
            {
                "type": "Button",
                "name": "button1",
                "caption": "Button 1"
            },
            {
                "type": "Button",
                "name": "button2",
                "caption": "Button 2"
            }
        ]
    }

---------------------------------------------------------------------------------------------------
## Column

Acts like `GroupBox` but does not have any frame.

![](doc/images/column.png)

## Settings Model

    {
        "type": "Column",
        "items": [
            {
                "type": "Button",
                "name": "button1",
                "caption": "Button 1"
            },
            {
                "type": "Button",
                "name": "button2",
                "caption": "Button 2"
            }
        ]
    }

[/proprietary]
