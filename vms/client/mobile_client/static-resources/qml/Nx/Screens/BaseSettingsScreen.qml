// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls
import Nx.Ui

Page
{
    default property alias data: contentColumn.data

    onLeftButtonClicked: Workflow.popCurrentScreen()
    topPadding: 4
    bottomPadding: 16

    Flickable
    {
        id: flickable

        anchors.fill: parent

        contentWidth: width
        contentHeight: contentColumn.height

        Column
        {
            id: contentColumn

            width: flickable.width
            spacing: 4
        }
    }
}
