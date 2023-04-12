// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml
import Qt.labs.qmlmodels

import Nx
import Nx.Core
import Nx.Controls
import Nx.Dialogs

import nx.vms.client.desktop

Control
{
    id: control
    property alias model: tableView.model

    contentItem: Column
    {
        HorizontalHeaderView
        {
            id: horizontalHeader
            syncView: tableView

            delegate: Rectangle
            {
                implicitWidth: Math.max(124, title.implicitWidth)
                implicitHeight: 28
                color: ColorTheme.colors.dark7

                Text
                {
                    id: title
                    x: 8
                    y: 6
                    text: display || ""
                    font.bold: true
                    color: ColorTheme.text
                }
            }
        }

        Rectangle
        {
            width: control.width
            height: 1
            color: ColorTheme.dark
        }

        TableView
        {
            id: tableView
            width: control.width
            height: control.height - y

            columnSpacing: 0
            rowSpacing: 0
            clip: true

            DelegateChooser
            {
                id: chooser
                role: "type"
                DelegateChoice
                {
                    roleValue: "checkbox"

                    Rectangle
                    {
                        implicitWidth: 28
                        implicitHeight: 28
                        color: ColorTheme.colors.dark7

                        CheckBox
                        {
                            x: 8
                            y: 6
                            // checked: tableView.model["checked"]
                        }
                    }
                }

                DelegateChoice
                {
                    roleValue: "text"

                    Rectangle
                    {
                        implicitWidth: Math.max(124, title.implicitWidth)
                        implicitHeight: 28
                        color: ColorTheme.colors.dark7

                        required property bool selected

                        Text
                        {
                            id: title
                            x: 8
                            y: 6
                            text: display || qsTr("NONE")
                            color: display ? ColorTheme.light : ColorTheme.colors.dark17
                        }
                    }
                }
            }

            delegate: chooser
        }
    }
}
