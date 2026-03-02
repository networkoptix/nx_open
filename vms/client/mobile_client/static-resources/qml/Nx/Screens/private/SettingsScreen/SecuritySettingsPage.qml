// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Controls
import Nx.Mobile.Popups
import Nx.Settings

import nx.vms.client.core

BaseSettingsPage
{
    title: qsTr("Security")

    LabeledSwitch
    {
        width: parent.width
        text: qsTr("Save Passwords")
        checkState: appContext.settings.savePasswords ? Qt.Checked : Qt.Unchecked
        extraText: qsTr("Automatically log in to servers")
        onClicked:
        {
            appContext.settings.savePasswords = checkState
            if (appContext.settings.savePasswords)
                return

            howToHandleSavedPasswordsDialog.open()
        }
    }

    Rectangle
    {
        width: parent.width
        height: serverCertCheckGroup.implicitHeight
        radius: 8
        color: ColorTheme.colors.dark6

        Column
        {
            id: serverCertCheckGroup

            anchors.fill: parent

            LabeledSwitch
            {
                id: securityOptionSwitch

                width: parent.width
                text: qsTr("Server Certificate Check")

                property int lastValidationLevel: Certificate.ValidationLevel.recommended

                checkState: appContext.settings.certificateValidationLevel
                    === Certificate.ValidationLevel.disabled
                        ? Qt.Unchecked
                        : Qt.Checked

                onClicked:
                {
                    if (checkState == Qt.Checked)
                    {
                        appContext.settings.certificateValidationLevel = lastValidationLevel
                    }
                    else
                    {
                        lastValidationLevel = appContext.settings.certificateValidationLevel
                        appContext.settings.certificateValidationLevel =
                            Certificate.ValidationLevel.disabled
                    }
                }

                Rectangle
                {
                    id: line

                    x: parent.leftPadding
                    width: parent.width - x - parent.rightPadding
                    anchors.bottom: parent.bottom

                    height: 1
                    color: ColorTheme.colors.dark8
                    visible: securityOptionSwitch.checkState === Qt.Checked
                }
            }


            StyledRadioButton
            {
                id: recommendedSecurityOptionRadioButton
                width: parent.width
                checked: appContext.settings.certificateValidationLevel
                    === Certificate.ValidationLevel.recommended
                text: qsTr("Recommended")
                extraText: qsTr("Your confirmation will be requested to pin self-signed certificates")
                visible: line.visible

                onClicked:
                {
                    appContext.settings.certificateValidationLevel =
                        Certificate.ValidationLevel.recommended
                }
            }

            StyledRadioButton
            {
                id: strictSecurityOptionRadioButton

                width: parent.width
                checked: appContext.settings.certificateValidationLevel
                    === Certificate.ValidationLevel.strict
                text: qsTr("Strict")
                extraText: qsTr("Connect only servers with public certificates")
                backgroundRadius: 8

                visible: line.visible

                onClicked:
                {
                    appContext.settings.certificateValidationLevel = Certificate.ValidationLevel.strict
                }
            }
        }
    }

    PopupBase
    {
        id: howToHandleSavedPasswordsDialog

        withCloseButton: false
        icon: "image://skin/48x48/Solid/warning.svg?primary=yellow"
        title: qsTr("How to handle saved passwords?")

        buttonBoxButtons:
        [
            PopupButton
            {
                text: qsTr("Keep")

                onClicked: howToHandleSavedPasswordsDialog.close()
            },

            PopupButton
            {
                text: qsTr("Delete")
                accented: true

                onClicked:
                {
                    appContext.credentialsHelper.clearSavedPasswords()
                    howToHandleSavedPasswordsDialog.close()
                }
            }
        ]
    }
}
