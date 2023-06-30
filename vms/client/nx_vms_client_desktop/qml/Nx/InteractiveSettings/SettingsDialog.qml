// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Controls
import Nx.Dialogs

import nx.vms.client.core
import nx.vms.client.desktop

Dialog
{
    id: dialog

    width: 400
    height: 200

    minimumWidth: 300
    minimumHeight: 100

    title: settingsModel && settingsModel.title || ""

    property var settingsModel: null
    property int padding: 16
    property int preferredHeight: 400

    function getValues()
    {
        return settingsView.getValues()
    }

    function adjust()
    {
        if (settingsView.implicitHeight === 0)
            return

        const previousHeight = height
        height = Math.min(content.implicitHeight + buttonBox.height, preferredHeight)
        minimumHeight = content.implicitHeight - settingsView.implicitHeight + buttonBox.height

        if (height > previousHeight)
            y -= (height - previousHeight) / 2
    }

    onSettingsModelChanged: settingsView.loadModel(settingsModel)

    component Header: RowLayout
    {
        spacing: 16

        Image
        {
            property var icons: ({
                "information": "image://svg/skin/standard_icons/sp_message_box_information_64.svg",
                "question": "image://svg/skin/standard_icons/sp_message_box_question_64.svg",
                "warning": "image://svg/skin/standard_icons/sp_message_box_warning_64.svg",
                "critical": "image://svg/skin/standard_icons/sp_message_box_critical_64.svg"
            })
            source: icons[settingsModel && settingsModel.icon] || icons["information"]
            Layout.alignment: Qt.AlignTop
        }

        ColumnLayout
        {
            spacing: 8

            Text
            {
                text: settingsModel && settingsModel.header || qsTr("Enter parameters")
                color: ColorTheme.light
                font.pixelSize: FontConfig.large.pixelSize
                font.weight: Font.Bold

                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            Text
            {
                text: settingsModel && settingsModel.description
                    || qsTr("This action requires some parameters to be filled.")

                color: ColorTheme.windowText
                font.pixelSize: FontConfig.normal.pixelSize

                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }
    }

    component Separator: Item
    {
        implicitHeight: 2
        implicitWidth: parent ? parent.width : 100

        Rectangle
        {
            width: parent.width
            height: 1
            color: ColorTheme.shadow
        }

        Rectangle
        {
            width: parent.width
            height: 1
            y: 1
            color: ColorTheme.dark
        }
    }

    ColumnLayout
    {
        id: content

        spacing: 0
        clip: true

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: buttonBox.top

        Header
        {
            Layout.fillWidth: true
            Layout.margins: dialog.padding
        }

        Separator { visible: !settingsView.isEmpty }

        Control
        {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 100
            Layout.minimumHeight: 20
            padding: dialog.padding
            visible: !settingsView.isEmpty

            contentItem: SettingsView
            {
                id: settingsView

                scrollBarParent: parent
            }
        }

        onImplicitHeightChanged: adjust()
    }

    buttonBox: DialogButtonBox
    {
        standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel
    }
}
