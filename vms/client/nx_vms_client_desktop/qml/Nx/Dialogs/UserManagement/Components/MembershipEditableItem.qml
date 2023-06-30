// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core
import Nx.Core.Controls
import Nx.Controls

import nx.vms.client.desktop

Rectangle
{
    id: checkableItem

    property string text: ""
    property string description: ""

    property bool cycle: false
    property var view: null

    readonly property bool selected: groupMouseArea.containsMouse || groupCheckbox.checked

    readonly property color selectedColor: selected
        ? ColorTheme.colors.light4
        : ColorTheme.colors.light10

    readonly property color descriptionColor: selected
        ? ColorTheme.colors.light10
        : ColorTheme.colors.light16

    height: 24

    color: groupMouseArea.containsMouse ? ColorTheme.colors.dark12 : "transparent"

    CheckBox
    {
        id: groupCheckbox

        spacing: 0
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.right: parent.right

        enabled: control.enabledProperty ? model[control.enabledProperty] : true
        checked: model[control.editableProperty]

        onCheckedChanged: model[control.editableProperty] = checked

        middleItem: IconImage
        {
            id: groupImage

            width: 20
            height: 20

            source: iconPath(model) + "?primary=light10&secondary=light4"
            sourceSize: Qt.size(width, height)

            opacity: groupCheckbox.enabled ? 1.0 : 0.3

            color: groupCheckbox.enabled
                ? checkableItem.selectedColor
                : ColorTheme.colors.light10
        }

        font: Qt.font({pixelSize: 12, weight: Font.Medium})

        colors.normal: checkableItem.cycle
            ? ColorTheme.colors.red_core
            : checkableItem.selectedColor

        textFormat: Text.StyledText
        middleSpacing: 0
        text:
        {
            const result = highlightMatchingText(checkableItem.text)
            if (control.currentSearchRegExp
                || !control.showDescription
                || !checkableItem.description)
            {
                return result
            }

            const description = NxGlobals.toHtmlEscaped(checkableItem.description)
            return `${result}<font color="${checkableItem.descriptionColor}">`
                + ` - ${description}</font>`
        }

        onVisualFocusChanged:
        {
            if (visualFocus && checkableItem.view)
                checkableItem.view.positionViewAtIndex(index, ListView.Contain)
        }
    }

    GlobalToolTip.text: model.toolTip || ""

    MouseArea
    {
        id: groupMouseArea

        anchors.fill: parent
        hoverEnabled: true

        onClicked:
            model[control.editableProperty] = !model[control.editableProperty]
    }
}
