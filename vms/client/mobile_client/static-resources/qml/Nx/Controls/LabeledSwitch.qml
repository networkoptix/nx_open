// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Window 2.0
import QtQuick.Controls 2.4

import Nx.Core 1.0

import "private"

Control
{
    id: control

    property alias text: textItem.text
    property alias checkState: indicator.checkState
    property alias icon: image.source
    property alias showIndicator: indicatorHolder.visible
    property alias showCustomArea: customAreaHolder.visible
    property bool interactive: true
    property bool manualStateChange: false
    property Item customArea: null
    property Item customTextAreaItem: null
    property var customAreaClickHandler

    property alias extraText: extraTextItem.text
    property alias extraTextColor: extraTextItem.color
    property color extraTextStandardColor: ColorTheme.colors.light12

    signal clicked()

    spacing: 16
    topPadding: 12
    bottomPadding: 12
    leftPadding: 16
    rightPadding: leftPadding

    implicitWidth: textItem.implicitWidth + indicatorHolder.width + spacing
        + leftPadding + rightPadding
        + d.customTextAreaWidth
    implicitHeight: topPadding + bottomPadding + centralArea.height

    background: Rectangle
    {
        color: ColorTheme.colors.dark6

        MaterialEffect
        {
            anchors.fill: parent
            clip: true
            rippleSize: 160
            mouseArea: backgroudMouseArea
        }

        MouseArea
        {
            id: backgroudMouseArea

            anchors.fill: parent
            onClicked:
            {
                if (!control.interactive)
                    return

                if (control.customAreaClickHandler)
                    control.customAreaClickHandler()
                else
                    control.handleSelectorClicked()
            }
        }
    }

    contentItem: Item
    {
        id: contentHolder

        implicitHeight: extraTextItem.visible
            ? d.kContentWithExtraTextHeight
            : d.kNormalContentHeight

        Image
        {
            id: image

            y: mainTextRow.y + (mainTextRow.height - height) / 2
            width: 24
            height: width

            visible: source != ""
            sourceSize.width: width * Screen.devicePixelRatio
            sourceSize.height: height * Screen.devicePixelRatio
        }

        Column
        {
            id: centralArea

            spacing: 4

            x: image.visible ? image.x + image.width + 8 : 0
            width: parent.width - (x + Math.max(rightControlsHolder.width, indicator.width) + 12)

            Row
            {
                id: mainTextRow

                width: parent.width
                spacing: 6

                Text
                {
                    id: textItem

                    width: Math.min(textItem.implicitWidth, parent.width - d.customTextAreaWidth)

                    color: ColorTheme.windowText
                    font.pixelSize: 16
                    wrapMode: Text.Text.Wrap
                }
            }

            Text
            {
                id: extraTextItem

                width: parent.width

                visible: text.length
                wrapMode: Text.Wrap
                font.pixelSize: 13
                color: control.extraTextStandardColor
            }
        }

        Row
        {
            id: rightControlsHolder

            y: -contentHolder.y
            height: control.height
            anchors.right: parent.right

            spacing: 12

            Row
            {
                id: customAreaHolder

                height: parent.height
                anchors.verticalCenter: parent.verticalCenter

                visible: control.customArea && control.showIndicator
            }

            Item
            {
                id: indicatorHolder

                height: control.height
                width: indicatorRow.width

                MouseArea
                {
                    onClicked: control.handleSelectorClicked()
                    height: parent.height
                    width: parent.width + control.rightPadding
                }

                Row
                {
                    id: indicatorRow

                    spacing: 12
                    anchors.verticalCenter: parent.verticalCenter

                    Rectangle
                    {
                        width: 1
                        height: 24
                        visible: control.showCustomArea
                        color: ColorTheme.colors.light16
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    SwitchIndicator
                    {
                        id: indicator
                        enabled: control.enabled
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }
    }

    function handleSelectorClicked()
    {
        if (!control.interactive)
            return

        if (control.showIndicator && !control.manualStateChange)
            indicator.toggle()

        control.clicked()
    }

    NxObject
    {
        id: d

        readonly property int kNormalContentHeight: 48
        readonly property int kContentWithExtraTextHeight: 68
        readonly property int customTextAreaWidth: customTextAreaItem
            ? customTextAreaItem.width + mainTextRow.spacing
            : 0
    }

    onCustomAreaChanged: customArea.parent = customAreaHolder

    onCustomTextAreaItemChanged: customTextAreaItem.parent = mainTextRow
}
