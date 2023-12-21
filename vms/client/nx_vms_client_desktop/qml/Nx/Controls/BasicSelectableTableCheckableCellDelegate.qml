// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Controls

Rectangle
{
    id: control

    required property bool selected

    implicitWidth: 28
    implicitHeight: 28
    color: selected ? ColorTheme.colors.brand_d6 : ColorTheme.colors.dark7

    CheckBox
    {
        anchors.centerIn: parent
        checked: model.checkState
        onCheckStateChanged:
        {
            if (checkState !== model.checkState)
                model.checkState = checkState
        }
    }
}
