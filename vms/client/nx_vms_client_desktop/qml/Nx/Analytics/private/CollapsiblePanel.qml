// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14
import QtQuick.Layouts 1.14
import QtQml 2.14

import Nx 1.0
import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

Column
{
    id: panel

    property alias name: nameButton.text
    property string value: ""

    property Item contentItem

    property bool displayValue: false

    property bool collapsed: false
    property bool autoCollapse: false

    property int animationDuration: 150

    spacing: 8

    signal clearRequested()

    onValueChanged:
    {
        if (!value)
            collapsed = false
        else if (autoCollapse)
            collapsed = true
    }

    Rectangle
    {
        id: line

        width: panel.width
        height: 1
        color: ColorTheme.transparent(ColorTheme.colors.dark8, 0.4)
    }

    Column
    {
        id: mainArea

        width: panel.width - panel.leftPadding - panel.rightPadding
        leftPadding: 12
        rightPadding: 12
        spacing: 8

        RowLayout
        {
            id: header

            width: mainArea.width - mainArea.leftPadding - mainArea.rightPadding
            spacing: 2

            TextButton
            {
                id: nameButton

                GlobalToolTip.text: truncated ? text : ""

                icon.source: "image://svg/skin/text_buttons/dropdown_arrow.svg"
                icon.width: 20
                icon.height: 20

                iconRotation: panel.collapsed ? -90 : 0
                Behavior on iconRotation { NumberAnimation { duration: panel.animationDuration }}

                color: panel.value ? ColorTheme.colors.light10 : ColorTheme.colors.light16
                hoveredColor: panel.value ? ColorTheme.colors.light7 : ColorTheme.colors.light13

                Layout.alignment: Qt.AlignBaseline
                Layout.minimumWidth: Math.min(implicitWidth, header.width / 2)
                Layout.maximumWidth: implicitWidth
                Layout.fillWidth: true

                onClicked:
                    panel.collapsed = !panel.collapsed
            }

            Text
            {
                id: valueText

                color: ColorTheme.text
                elide: Text.ElideRight

                text: panel.displayValue ? panel.value : ""
                visible: !!text

                Layout.alignment: Qt.AlignBaseline | Qt.AlignRight
                Layout.maximumWidth: header.width - nameButton.Layout.minimumWidth
                    - clearButton.width - 12
            }

            TextButton
            {
                id: clearButton

                icon.source: "qrc:///skin/text_buttons/clear.png"
                color: hovered ? ColorTheme.colors.light4 : ColorTheme.colors.light16
                visible: !!panel.value
                icon.width: 20
                icon.height: 20
                Layout.alignment: Qt.AlignRight

                onClicked:
                    panel.clearRequested()
            }
        }

        Item
        {
            id: contentHolder

            width: mainArea.width - mainArea.leftPadding - mainArea.rightPadding

            height: (panel.contentItem && !panel.collapsed)
                ? panel.contentItem.implicitHeight
                : 0

            clip: true

            Behavior on height
            {
                id: heightAnimation
                enabled: false

                NumberAnimation
                {
                    duration: panel.animationDuration
                    easing.type: Easing.InOutQuad
                }
            }

            Connections
            {
                target: panel.contentItem

                function onImplicitHeightChanged()
                {
                    // Disable resize animation if content was resized internally.
                    heightAnimation.enabled = false
                }
            }
        }
    }

    Binding
    {
        target: contentItem
        property: "parent"
        value: contentHolder
    }

    Binding
    {
        target: contentItem
        property: "width"
        value: contentHolder.width
    }

    Binding
    {
        target: contentItem
        property: "height"
        value: contentHolder.height
    }

    onCollapsedChanged:
    {
        // Enable resize animation when the panel is collapsed or expanded.
        heightAnimation.enabled = true
    }
}
