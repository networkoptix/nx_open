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
            contentHeight: column.implicitHeight
            clip: true

            topPadding: 36
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
