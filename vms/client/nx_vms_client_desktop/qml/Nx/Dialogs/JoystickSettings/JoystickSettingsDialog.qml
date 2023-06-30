// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.11
import QtQml 2.14

import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Dialogs 1.0

import nx.vms.client.desktop 1.0

Dialog
{
    id: control

    property JoystickButtonSettingsModel buttonSettingsModel: null
    property JoystickButtonActionChoiceModel buttonBaseActionChoiceModel: null
    property JoystickButtonActionChoiceModel buttonModifiedActionChoiceModel: null
    property FilteredResourceProxyModel layoutModel: null
    property bool joystickIsSupported: true
    property string joystickName: ""
    property bool connectedToServer: false
    property bool applyButtonEnabled: false

    property bool panAndTiltHighlighted: false
    property bool zoomHighlighted: false

    signal resetToDefault()

    title: qsTr("Joystick Settings")

    width: minimumWidth
    height: 825
    minimumWidth: tabControl.implicitWidth
    minimumHeight: 407

    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0

    onApplyButtonEnabledChanged:
    {
        const applyButton = buttonBox.standardButton(DialogButtonBox.Apply)
        if (applyButton)
            applyButton.enabled = applyButtonEnabled
    }

    contentItem: Rectangle
    {
        id: backgroundRect

        color: ColorTheme.colors.dark7

        Rectangle
        {
            id: labelRect

            width: parent.width
            height: 60

            color: ColorTheme.colors.dark8

            Label
            {
                id: nameLabel

                x: 16
                anchors.verticalCenter: parent.verticalCenter

                elide: Text.ElideRight
                font.pixelSize: 22
                font.weight: Font.Medium
                color: ColorTheme.colors.light4

                text: control.joystickName.includes("joystick")
                    ? joystickName
                    : "%1 %2".arg(control.joystickName).arg(qsTr("joystick"))
            }
        }

        StackLayout
        {
            anchors.top: labelRect.bottom
            anchors.topMargin: 8
            anchors.bottom: parent.bottom
            width: parent.width

            currentIndex: control.joystickIsSupported ? 0 : 1

            TabControl
            {
                id: tabControl

                implicitWidth: tabContent.implicitWidth

                width: parent.width
                height: parent.height

                tabBar.spacing: 20

                Tab
                {
                    button: CompactTabButton
                    {
                        height: tabControl.tabBar.height

                        topPadding: 10
                        bottomPadding: 9

                        compact: false
                        text: qsTr("Basic Actions")
                    }

                    page: JoystickSettingsTab
                    {
                        id: tabContent

                        model: control.buttonSettingsModel
                        buttonBaseActionChoiceModel: control.buttonBaseActionChoiceModel
                        buttonModifiedActionChoiceModel: control.buttonModifiedActionChoiceModel
                        layoutModel: control.layoutModel
                        infoBarVisible: !control.connectedToServer

                        panAndTiltHighlighted: control.panAndTiltHighlighted
                        zoomHighlighted: control.zoomHighlighted
                    }
                }

                Tab
                {
                    visible: control.buttonSettingsModel
                        && control.buttonSettingsModel.modifierButtonName

                    button: CompactTabButton
                    {
                        height: tabControl.tabBar.height

                        topPadding: 10
                        bottomPadding: 9

                        compact: false
                        text: qsTr("With Modifier")
                    }

                    page: JoystickSettingsTab
                    {
                        basicMode: false

                        model: control.buttonSettingsModel
                        buttonBaseActionChoiceModel: control.buttonBaseActionChoiceModel
                        buttonModifiedActionChoiceModel: control.buttonModifiedActionChoiceModel
                        layoutModel: control.layoutModel
                        infoBarVisible: !control.connectedToServer

                        panAndTiltHighlighted: control.panAndTiltHighlighted
                        zoomHighlighted: control.zoomHighlighted
                    }
                }
            }

            Item
            {
                width: parent.width
                height: parent.height

                Label
                {
                    anchors.centerIn: parent

                    font.pixelSize: 16
                    color: ColorTheme.colors.light16

                    visible: !control.joystickIsSupported

                    text: qsTr("This model is not supported")
                }
            }
        }
    }

    buttonBox: DialogButtonBox
    {
        buttonLayout: DialogButtonBox.KdeLayout

        standardButtons: control.joystickIsSupported
            ? DialogButtonBox.Ok | DialogButtonBox.Apply | DialogButtonBox.Cancel
            : DialogButtonBox.Ok

        onStandardButtonsChanged:
        {
            const applyButton = buttonBox.standardButton(DialogButtonBox.Apply)
            if (applyButton)
                applyButton.enabled = false
        }
    }

    Button
    {
        x: 16
        anchors.verticalCenter: buttonBox.verticalCenter

        visible: control.joystickIsSupported

        text: qsTr("Reset to Default")

        iconUrl: "image://svg/skin/joystick_settings/reset.svg"

        onClicked: control.resetToDefault()
    }
}
