// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Core.Controls
import Nx.Controls

Control
{
    id: control

    property alias text: title.text
    property bool closeable: true
    property alias action: actionButton.action
    property bool shown: false

    property alias textColor: title.color
    property alias backgroundColor: background.color
    property alias closeButtonColor: closeButton.icon.color
    property alias actionButtonColor: actionButton.textColor
    property alias actionButtonFont: actionButton.font
    property alias borderColor: border.color
    property alias borderWidth: border.height
    property alias iconItem: icon.contentItem

    signal closed()
    signal closeClicked()

    padding: 20
    clip: true

    state: shown ? "visible" : "hidden"
    visible: opacity > 0

    states:
    [
        State
        {
            name: "visible"
            PropertyChanges { control.opacity: 1 }
        },
        State
        {
            name: "hidden"
            PropertyChanges { control.opacity: 0 }
        }
    ]

    transitions:
    [
        Transition
        {
            from: "visible"
            to: "hidden"

            SequentialAnimation
            {
                NumberAnimation { property: "opacity"; duration: 160 }
                ScriptAction { script: control.closed() }
            }
        },
        Transition
        {
            from: "hidden"
            to: "visible"

            NumberAnimation { property: "opacity"; duration: 80 }
        }
    ]

    background: Rectangle
    {
        id: background

        Rectangle
        {
            id: border

            anchors.bottom: parent.bottom
            width: parent.width
        }

        MultiPointTouchArea { anchors.fill: parent }
    }

    contentItem: GridLayout
    {
        columns: 3

        rowSpacing: 2
        columnSpacing: 8

        Control
        {
            id: icon

            Layout.alignment: Qt.AlignTop
        }

        Text
        {
            id: title

            font.pixelSize: 16
            wrapMode: Text.Wrap
            maximumLineCount: 3
            elide: Text.ElideRight
            lineHeightMode: Text.FixedHeight
            lineHeight: 24

            Layout.fillWidth: true
        }

        IconButton
        {
            id: closeButton

            visible: control.closeable
            icon.source: "image://skin/24x24/Outline/close.svg?primary=dark1"
            icon.width: 24
            icon.height: 24
            compact: true

            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            Layout.alignment: Qt.AlignTop

            onClicked: control.closeClicked()
        }

        TextButton
        {
            id: actionButton

            visible: !!control.action

            topPadding: 0
            leftPadding: 0
            rightPadding: 0
            bottomPadding: 0

            font.weight: Font.Normal
            font.underline: true

            Layout.row: 1
            Layout.column: 1
            Layout.preferredHeight: 24
        }
    }
}
