// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import Nx.Ui 1.0

Page
{
    id: screen

    property string cloudSystemId
    property alias systemName: screen.title
    property alias login: loginTextField.text

    signal gotResult(value: string)

    objectName: "digestLoginToCloudScreen"

    onLeftButtonClicked:
    {
        passwordTextField.enabled = false //< Supress empty password warning under the field.
        Workflow.popCurrentScreen()
        Workflow.openSessionsScreen()
    }

    Column
    {
        width: parent.width
        spacing: 8
        leftPadding: 8
        rightPadding: 8

        property real availableWidth: width - leftPadding - rightPadding

        Column
        {
            spacing: 8
            leftPadding: 8
            rightPadding: 8
            width: parent.width - parent.leftPadding - parent.rightPadding

            WarningPanel
            {
                width: screen.width
                opened: true
                backgroundColor: ColorTheme.colors.brand_d5
                anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
                enableAnimation: false

                text: qsTr("Enter your %1 account password to connect to the Site",
                    "%1 is the short cloud name (like 'Cloud')")
                    .arg(applicationInfo.shortCloudName())
            }

            TextField
            {
                id: loginTextField

                enabled: false
                width: parent.width - parent.leftPadding - parent.rightPadding
            }

            PasswordTextField
            {
                id: passwordTextField

                width: parent.width - parent.leftPadding - parent.rightPadding
            }
        }

        LoginButton
        {
            width: parent.availableWidth

            onClicked:
            {
                if (passwordTextField.hasError)
                    return

                screen.gotResult(passwordTextField.password)
                Workflow.popCurrentScreen()
            }
        }
    }

    Component.onCompleted:
    {
        CoreUtils.executeLater(function() { passwordTextField.forceActiveFocus() }, this)
    }
}
