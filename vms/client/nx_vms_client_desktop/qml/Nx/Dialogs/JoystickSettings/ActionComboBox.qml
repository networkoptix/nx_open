// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4

import Nx 1.0
import Nx.Controls 1.0
import Nx.Models 1.0

ComboBox
{
    id: control

    property real maxDelegateWidth: 0

    textRole: "name"

    delegate: ItemDelegate
    {
        height: isSeparator ? 8 : 24
        width: control.maxDelegateWidth

        enabled: !isSeparator && isEnabled

        highlighted: control.highlightedIndex === index

        background: Rectangle
        {
            color: highlightedIndex === index ? ColorTheme.colors.brand_core : ColorTheme.midlight
        }

        contentItem: Item
        {
            id: popupItem

            readonly property int setButtonsIndent: 40
            implicitWidth: actionName.leftPadding + actionName.contentWidth +
                setButtonsIndent +
                setButtons.contentWidth + setButtons.rightPadding

            anchors.fill: parent

            Component.onCompleted:
            {
                if (implicitWidth > maxDelegateWidth)
                    maxDelegateWidth = implicitWidth
            }

            CompactMenuSeparator
            {
                anchors.fill: parent

                visible: isSeparator
            }

            Item
            {
                anchors.fill: parent

                visible: !isSeparator
                enabled: model.isEnabled

                Text
                {
                    id: actionName

                    anchors.left: parent.left
                    leftPadding: 8 + (tab * control.tabSize)
                    height: parent.height

                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    color: highlightedIndex === index
                        ? ColorTheme.highlightContrast
                        : ColorTheme.text
                    opacity: isEnabled ? 1 : 0.3

                    readonly property real tab: control.tabRole ? itemData[control.tabRole] : 0

                    text: name
                }

                Text
                {
                    id: setButtons

                    anchors.right: parent.right
                    rightPadding: 8
                    height: parent.height

                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    color: highlightedIndex === index
                        ? ColorTheme.colors.brand_contrast
                        : ColorTheme.lighter(ColorTheme.windowText, 4)
                    opacity: isEnabled ? 1 : 0.3

                    readonly property real tab: control.tabRole ? itemData[control.tabRole] : 0

                    text: buttons
                }
            }
        }
    }

    popup: Popup
    {
        y: control.height
        width: control.maxDelegateWidth

        implicitHeight: contentItem.implicitHeight + 4

        padding: 0
        topPadding: 2
        bottomPadding: 2

        background: Rectangle
        {
            color: ColorTheme.midlight
            radius: 2
        }

        contentItem: Loader
        {
            sourceComponent: control.view
        }
    }

    property var modelDataAccessor: ModelDataAccessor
    {
        model: control.model

        onRowsInserted:
        {
            if (control.currentIndex >= first)
                control.currentIndex += last - first + 1
        }

        onRowsRemoved:
        {
            if (control.currentIndex >= first)
                control.currentIndex -= last - first + 1
        }
    }
}
