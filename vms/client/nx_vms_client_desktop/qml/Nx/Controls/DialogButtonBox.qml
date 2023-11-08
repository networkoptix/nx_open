// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0
import QtQuick.Templates 2.12 as T

import Nx 1.0
import Nx.Controls 1.0
import Nx.Core 1.0

T.DialogButtonBox
{
    id: control

    property var accentRoles: [T.DialogButtonBox.YesRole, T.DialogButtonBox.AcceptRole]

    implicitWidth: Math.max(
        implicitBackgroundWidth + leftInset + rightInset,
        (control.count === 1 ? contentWidth * 2 : contentWidth)
            + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
        contentHeight + topPadding + bottomPadding)
    contentWidth: contentItem.implicitWidth

    width: parent ? parent.width : 0
    anchors.bottom: parent ? parent.bottom : undefined

    alignment: Qt.AlignRight
    spacing: 8
    padding: 16

    background: Rectangle
    {
        implicitHeight: 60
        color: ColorTheme.colors.dark7

        Rectangle
        {
            width: parent.width
            height: 1
            color: ColorTheme.shadow
        }
        Rectangle
        {
            width: parent.width
            height: 1
            y: 1
            color: ColorTheme.dark
        }
    }

    contentItem: Row
    {
        spacing: control.spacing

        Repeater { model: control.contentModel }
    }


    delegate: Button
    {
        width: Math.max(implicitWidth, 80)
        isAccentButton: T.DialogButtonBox.buttonRole === accentRoles
            || accentRoles.indexOf(T.DialogButtonBox.buttonRole) !== -1
    }
}
