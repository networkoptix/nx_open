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
    readonly property string decorationPath: model.decoration ?? ""

    signal clicked()
    signal doubleClicked()

    implicitWidth: Math.max(contentRowLayout.implicitWidth, 28)
    implicitHeight: Math.max(contentRowLayout.implicitHeight, 28)
    color: control.selected ? ColorTheme.colors.dark9 : ColorTheme.colors.dark7

    RowLayout
    {
        id: contentRowLayout
        anchors.fill: parent
        spacing: 8

        Loader
        {
            visible: !!model.decoration
            sourceComponent: decorationPath.startsWith("image://resource/")
                ? resourceImage
                : coloredImage
        }

        Component
        {
            id: resourceImage

            Image
            {
                sourceSize.width: 20
                sourceSize.height: 20
                source: control.decorationPath
            }
        }

        Component
        {
            id: coloredImage

            ColoredImage
            {
                sourceSize.width: 20
                sourceSize.height: 20
                sourcePath: control.decorationPath
            }
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
