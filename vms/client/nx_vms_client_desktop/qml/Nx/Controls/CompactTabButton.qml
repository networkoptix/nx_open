import QtQuick 2.6
import QtQuick.Controls 2.4
import QtQuick.Controls.impl 2.4

import Nx 1.0

/**
 * TabButton that shows icon and text when selected, and only icon when not selected.
 * The transition is animated.
 *
 * Used palette entries:
 *     ButtonText - normal color
 *     BrightText - hovered color
 *     HighlightedText - selected color
 *     Mid - hover underline color
 */
TabButton
{
    id: tabButton

    property int animationDurationMs: 200

    readonly property bool isCurrent: TabBar.tabBar && TabBar.index === TabBar.tabBar.currentIndex

    readonly property color color: isCurrent
        ? palette.highlightedText
        : (hovered && !pressed) ? palette.brightText : palette.buttonText

    contentItem: Item
    {
        id: content

        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight

        Row
        {
            id: row

            spacing: tabButton.spacing

            IconImage
            {
                id: image

                source: tabButton.icon.source
                color: tabButton.color
                name: tabButton.icon.name
                anchors.verticalCenter: parent.verticalCenter
            }

            Text
            {
                id: text

                text: tabButton.text
                font: tabButton.font
                color: tabButton.color
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    background: Item
    {
        Rectangle
        {
            id: underline

            x: content.x
            width: content.implicitWidth
            height: 1
            anchors.bottom: parent.bottom
            visible: tabButton.isCurrent || tabButton.hovered
            color: tabButton.isCurrent ? palette.highlightedText : palette.mid
        }
    }

    leftPadding: 0
    rightPadding: 0
    topPadding: 6
    bottomPadding: 6
    spacing: 4

    width: isCurrent ? implicitWidth : (leftPadding + image.width + spacing)
    clip: true

    Behavior on width
    {
        NumberAnimation
        {
            duration: animationDurationMs
            easing.type: Easing.InOutCubic
        }
    }
}
