// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQml
import QtQuick
import QtQml

import Nx.Controls
import Nx.Core

import nx.vms.client.desktop

Text
{
    id: root

    property int row: model.row
    property int column: model.column
    property bool isElided: width < implicitWidth
    property bool hovered: TableView.view ? TableView.view.hoveredRow === row : false

    elide: Text.ElideRight

    font.pixelSize: 14

    text: model.display ?? ""

    color: model.foregroundColor ?? ColorTheme.colors.light4

    GlobalToolTip.text:
    {
        const toolTip = model.toolTip
        if (toolTip)
            return toolTip

        if (root.isElided)
            return root.text

        return ""
    }
}
