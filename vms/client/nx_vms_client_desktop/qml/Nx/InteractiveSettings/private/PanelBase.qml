// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Controls

FocusScope
{
    id: informationPanel

    property alias headerItem: headerControl.contentItem
    property alias description: description.text
    property alias sourceItem: sourceControl.contentItem
    property alias content: content.contentItem
    property alias permissions: permissions
    property var request

    property bool approveButtonVisible: !!request
    property bool rejectButtonVisible: false
    property bool contentVisible: true

    signal approveClicked(string authCode)
    signal rejectClicked()

    implicitWidth: column.implicitWidth
    implicitHeight: column.implicitHeight

    onRequestChanged: authCode.clear()
    onVisibleChanged:
    {
        authCode.clear()
        permissions.collapsed = true
    }

    Control
    {
        id: column
        padding: 12
        background: Rectangle { color: ColorTheme.colors.dark8 }
        anchors.fill: parent

        contentItem: ColumnLayout
        {
            spacing: 16

            clip: true

            Column
            {
                spacing: 8
                Layout.fillWidth: true

                Control
                {
                    id: headerControl

                    width: parent.width
                }

                Text
                {
                    id: description

                    font.pixelSize: 14
                    color: ColorTheme.windowText
                    width: parent.width
                    visible: !!text
                    wrapMode: Text.Wrap
                }
            }

            Control
            {
                id: sourceControl

                Layout.fillWidth: true
                visible: !!contentItem
            }

            Permissions
            {
                id: permissions
                visible: !!permission
            }

            Column
            {
                spacing: 8
                visible: !!request && !!request.pinCode

                Text
                {
                    text: qsTr("Integration pairing code")
                    color: ColorTheme.light
                    font.pixelSize: 14
                }

                AuthCode
                {
                    id: authCode
                    onValidChanged:
                    {
                        if (valid)
                            approveButton.focus = true
                    }
                }
            }

            Row
            {
                spacing: 8
                visible: approveButtonVisible || rejectButtonVisible

                Button
                {
                    id: approveButton
                    visible: approveButtonVisible
                    text: qsTr("Approve")
                    enabled: authCode.valid || !authCode.visible
                    onClicked: informationPanel.approveClicked(authCode.value)
                }

                Button
                {
                    visible: rejectButtonVisible
                    text: qsTr("Reject")
                    onClicked: informationPanel.rejectClicked()
                }
            }

            Rectangle
            {
                id: separator

                Layout.fillWidth: true
                height: 1
                color: ColorTheme.colors.dark13
                visible: content.contentItem && content.contentItem.visible
            }

            Control
            {
                id: content

                Layout.fillWidth: true
                width: parent.width
                visible: contentItem && contentVisible
            }
        }
    }
}
