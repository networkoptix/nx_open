// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQml

import Nx.Controls

import "private"

/**
 * Interactive Settings type.
 */
Group
{
    id: control

    property string name: ""
    property string caption: ""
    property string description: ""
    property string style: defaultStyle
    property bool useSwitchButton: false
    property var defaultValue
    property var value: defaultValue
    property bool isActive: false
    property var parametersModel

    property string switchButtonBehavior: "disable"

    property int depth: 1
    readonly property string defaultStyle: depth === 1 ? "panel" : "group"

    property bool isGroup: true
    property bool fillWidth: true

    childrenItem: groupBox.layout

    implicitWidth: groupBox.implicitWidth
    implicitHeight: groupBox.implicitHeight

    component GroupBoxItem: GroupBox
    {
        title: control.caption

        contentItem: LabeledColumnLayout
        {
            layoutControl: control
        }
    }

    Component
    {
        id: groupStyle
        GroupBoxItem { }
    }

    Component
    {
        id: labelStyle
        GroupBoxItem { background: null }
    }

    Component
    {
        id: panelStyle

        Panel
        {
            id: panel

            title: control.caption
            checkable: control.useSwitchButton
            checked: control.value ?? defaultValue ?? true
            width: parent.width
            switchButtonBehavior: ({
                "none": Panel.SwitchButtonBehavior.None,
                "hide": Panel.SwitchButtonBehavior.Hide,
                "disable": Panel.SwitchButtonBehavior.Disable,
            })[control.switchButtonBehavior] ?? Panel.SwitchButtonBehavior.None

            onTriggered: (checked) => control.value = checked

            topPadding: contentHeight > 0 ? 36 : 0

            bottomPadding: 16

            contentItem: LabeledColumnLayout
            {
                id: column
                layoutControl: control
            }
        }
    }

    contentItem: Loader
    {
        id: groupBox

        property var layout: item && item.contentItem

        readonly property var styles: ({
            "panel": panelStyle,
            "group": groupStyle,
            "label": labelStyle,
        })

        sourceComponent: styles[style] ?? styles[defaultStyle]
    }
}
