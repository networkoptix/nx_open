// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Core
import Nx.Controls
import Nx.Dialogs

Dialog
{
    id: dialog
    objectName: "batchUserEditDialog" //< For autotesting.

    modality: Qt.WindowModal

    color: ColorTheme.colors.dark7

    readonly property int enableUsers: enableUsersComboBox.currentValue
    property bool enableUsersEditable: true
    readonly property int disableDigest: disableDigestComboBox.currentValue
    property bool digestEditable: true
    property int usersCount: 0

    title: qsTr("Batch Edit - %n Users", "", usersCount)

    fixedWidth: implicitWidth
    fixedHeight: implicitHeight

    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0

    contentItem: Column
    {
        id: content

        Rectangle
        {
            color: ColorTheme.colors.dark8
            width: content.width
            height: 60

            Text
            {
                anchors.fill: parent
                color: ColorTheme.colors.light4
                font: Qt.font({pixelSize: 24, weight: Font.Medium})
                text: qsTr("%n Users", "", dialog.usersCount)
                verticalAlignment: Qt.AlignVCenter
                padding: 20
            }
        }

        Column
        {
            leftPadding: 16
            topPadding: 16
            rightPadding: 16
            bottomPadding: 16

            GridLayout
            {
                columns: 2
                columnSpacing: 8
                rowSpacing: 8

                Label
                {
                    text: qsTr("User status", "Whether users are enabled or disabled")
                    visible: dialog.enableUsersEditable
                    Layout.alignment: Qt.AlignRight | Qt.AlignBaseline
                }

                ComboBox
                {
                    id: enableUsersComboBox

                    textRole: "text"
                    valueRole: "value"
                    visible: dialog.enableUsersEditable
                    Layout.alignment: Qt.AlignLeft | Qt.AlignBaseline

                    model: ListModel
                    {
                        ListElement
                        {
                            text: `<${qsTr("keep current value")}>`
                            value: Qt.PartiallyChecked
                        }

                        ListElement { text: qsTr("Enabled"); value: Qt.Checked }
                        ListElement { text: qsTr("Disabled"); value: Qt.Unchecked }
                    }
                }

                Label
                {
                    text: qsTr("Insecure (digest) authentication")
                    visible: dialog.digestEditable
                    Layout.alignment: Qt.AlignRight | Qt.AlignBaseline
                }

                ComboBox
                {
                    id: disableDigestComboBox

                    textRole: "text"
                    valueRole: "value"
                    visible: dialog.digestEditable
                    Layout.alignment: Qt.AlignLeft | Qt.AlignBaseline

                    model: ListModel
                    {
                        ListElement { text: qsTr("Do not change"); value: Qt.PartiallyChecked }
                        ListElement { text: qsTr("Disable"); value: Qt.Unchecked }
                    }
                }
            }
        }
    }

    buttonBox: DialogButtonBox
    {
        id: buttonBox

        standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel
        buttonLayout: DialogButtonBox.KdeLayout
    }
}
