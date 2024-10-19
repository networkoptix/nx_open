// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6

import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Settings 1.0
import Nx.Ui 1.0

import nx.vms.client.core 1.0

Page
{
    id: screen

    onLeftButtonClicked: Workflow.popCurrentScreen()
    title: qsTr("Security")
    Column
    {
        spacing: 4
        width: parent.width

        StyledRadioButton
        {
            width: parent.width
            checked: !strictChoice.checked
            text: qsTr("Recommended")
            extraText: qsTr("Your confirmation will be requested to pin self-signed certificates")
        }

        StyledRadioButton
        {
            id: strictChoice

            width: parent.width
            checked: settings.certificateValidationLevel == Certificate.ValidationLevel.strict
            text: qsTr("Strict")
            extraText: qsTr("Connect only servers with public certificates")

            onCheckedChanged:
            {
                settings.certificateValidationLevel = strictChoice.checked
                    ? Certificate.ValidationLevel.strict
                    : Certificate.ValidationLevel.recommended
            }
        }
    }
}
