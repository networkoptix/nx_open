// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Templates as T

import Nx.Core
import Nx.Core.Controls

import "private"

// TODO #ynikitenkov Add "support text" functionality support.
T.ComboBox
{
    id: control

    property alias backgroundMode: fieldBackground.mode
    property alias labelText: fieldBackground.labelText

    implicitWidth: 268
    implicitHeight: 56

    font.pixelSize: 16
    font.weight: 500

    spacing: 8

    topPadding: 30
    leftPadding: 12
    rightPadding: control.spacing
        + indicatorImage.width
        + indicatorImage.anchors.rightMargin

    background: FieldBackground
    {
        id: fieldBackground

        owner: control
        compactLabelMode: !!textItem.text
    }

    contentItem: Text
    {
        id: textItem

        font: control.font
        color: enabled
            ? ColorTheme.colors.light4
            : ColorTheme.transparent(ColorTheme.colors.light4, 0.3)
        elide: Text.ElideRight

        text: control.displayText
    }

    indicator: ColoredImage
    {
        id: indicatorImage

        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter

        opacity: enabled ? 1 : 0.3

        sourcePath: control.popup.visible
            ? "image://skin/20x20/Outline/arrow_up.svg?primary=light4"
            : "image://skin/20x20/Outline/arrow_down.svg?primary=light4"
        sourceSize: Qt.size(20, 20)
    }

    delegate: PopupItemDelegate
    {
        selected: control.currentIndex === index
        textRole: control.textRole
    }

    popup: Popup
    {
        readonly property int kMaxElementsCount: 5

        width: control.width
        height: Math.min(contentItem.implicitHeight, kMaxElementsCount * 56)

        modal: true
        padding: 0
        background: Rectangle
        {
            color: ColorTheme.colors.dark7
        }

        contentItem: ListView
        {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
        }
    }
}
