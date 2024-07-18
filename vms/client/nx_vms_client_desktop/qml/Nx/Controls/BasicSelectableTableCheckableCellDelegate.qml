// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Controls

Rectangle
{
    id: control

    required property bool selected
    property bool hovered: TableView.view ? TableView.view.hoveredRow === row : false

    implicitWidth: 28
    implicitHeight: 28
    color: selected ? ColorTheme.colors.dark9 : (hovered ? ColorTheme.colors.dark8 : ColorTheme.colors.dark7)

    CheckBox
    {
        anchors.centerIn: parent
        checked: model.checkState
        onCheckStateChanged:
        {
            control.TableView.view.focus = true
            if (checkState !== model.checkState)
                model.checkState = checkState
        }
    }
}
