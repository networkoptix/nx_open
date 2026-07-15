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

    enum Type { Info, Success, Warning, Error }

    property int type: Banner.Info
    property alias text: title.text
    property bool closeable: true
    property alias action: actionButton.action
    property bool shown: false

    property color foregroundColor: ColorTheme.colors.dark1
    property color backgroundColor: styles[type].backgroundColor
    property string iconPath: styles[type].iconPath

    readonly property var styles: ({
        [Banner.Info]: {
            backgroundColor: ColorTheme.colors.blue,
            iconPath: "image://skin/24x24/Solid/info.svg",
        },
        [Banner.Success]: {
            backgroundColor: ColorTheme.colors.green_attention,
            iconPath: "image://skin/24x24/Solid/success.svg",
        },
        [Banner.Warning]: {
            backgroundColor: ColorTheme.colors.yellow_attention,
            iconPath: "image://skin/24x24/Solid/warning.svg",
        },
        [Banner.Error]: {
            backgroundColor: ColorTheme.colors.red_attention,
            iconPath: "image://skin/24x24/Solid/error.svg",
        }
    })

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
        color: control.backgroundColor

        Rectangle
        {
            anchors.bottom: parent.bottom
            width: parent.width

            height: 1
            color: ColorTheme.colors.dark9
        }

        MultiPointTouchArea { anchors.fill: parent }
    }

    contentItem: GridLayout
    {
        columns: 3

        rowSpacing: 2
        columnSpacing: 8

        ColoredImage
        {
            id: icon

            sourcePath: control.iconPath
            primaryColor: control.foregroundColor
            sourceSize: Qt.size(24, 24)

            Layout.alignment: Qt.AlignTop
        }

        Text
        {
            id: title

            color: control.foregroundColor
            font.pixelSize: 16
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight

            Layout.fillWidth: true
        }

        IconButton
        {
            id: closeButton

            visible: control.closeable
            compact: true

            icon.source: "image://skin/24x24/Outline/close.svg?primary=dark1"
            icon.width: 24
            icon.height: 24

            onClicked: control.closeClicked()

            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            Layout.alignment: Qt.AlignTop
        }

        TextButton
        {
            id: actionButton

            visible: !!control.action

            topPadding: 0
            leftPadding: 0
            rightPadding: 0
            bottomPadding: 0

            textColor: control.foregroundColor
            font.weight: Font.Normal
            font.underline: true

            Layout.row: 1
            Layout.column: 1
            Layout.preferredHeight: 24
        }
    }
}
