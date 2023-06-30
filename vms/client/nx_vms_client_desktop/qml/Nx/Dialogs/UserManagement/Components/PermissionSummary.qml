// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Layouts 1.15

import Nx.Core 1.0

Grid
{
    columns: 2
    flow: Grid.TopToBottom

    columnSpacing: 50

    property alias model: repeater.model

    Text
    {
        text: qsTr("No custom permissions")

        visible: repeater.count === 0
        font: Qt.font({pixelSize: 14, weight: Font.Normal})
        color: ColorTheme.colors.light16
    }

    Repeater
    {
        id: repeater

        delegate: RowLayout
        {
            spacing: 4

            Image
            {
                width: 20
                height: 20
                source: model.checkState == Qt.Checked ? "image://svg/skin/user_settings/ok.svg" : ""
                sourceSize: Qt.size(width, height)
            }

            Text
            {
                font: Qt.font({pixelSize: 14, weight: Font.Normal})
                color: model.checkState == Qt.Checked
                    ? ColorTheme.colors.light10
                    : ColorTheme.colors.dark13
                text: (model && model.display) || ""
            }
        }
    }
}
