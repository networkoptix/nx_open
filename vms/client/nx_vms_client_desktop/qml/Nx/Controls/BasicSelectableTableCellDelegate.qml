// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Controls
import Nx.Core

import nx.vms.client.desktop

Rectangle
{
    id: control

    required property bool selected

    signal clicked()
    signal doubleClicked()

    implicitWidth: Math.max(contentRowLayout.implicitWidth, 28)
    implicitHeight: Math.max(contentRowLayout.implicitHeight, 28)
    color: control.selected ? ColorTheme.colors.brand_d6 : ColorTheme.colors.dark7

    RowLayout
    {
        id: contentRowLayout
        anchors.fill: parent
        spacing: 8

        Image
        {
            id: image

            sourceSize.width: 20
            sourceSize.height: 20

            source: model.decoration ?? ""
            visible: !!model.decoration
        }

        BasicTableCellDelegate
        {
            id: basicTableCellDelegate
            Layout.fillWidth: true
        }
    }

    MouseArea
    {
        anchors.fill: parent
        onClicked:
        {
            if (control.TableView.view.selectionBehavior === TableView.SelectRows)
            {
                control.TableView.view.selectionModel.select(
                    control.TableView.view.index(row, column),
                    ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Rows)
                control.TableView.view.focus = true
            }
            control.clicked()
        }

        onDoubleClicked: control.doubleClicked()
    }
}
