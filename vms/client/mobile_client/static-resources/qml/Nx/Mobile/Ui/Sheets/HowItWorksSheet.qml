// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Controls
import Nx.Mobile.Controls

BottomSheet
{
    id: control

    property alias description: descriptionTextItem.text
    property alias doNotShowAgain: doNotShowAgainCheckBox.checked
    property string continueButtonText: qsTr("Continue")
    title: qsTr("How it works")

    signal cancelled()
    signal continued()

    Text
    {
        id: descriptionTextItem

        width: parent.width
        wrapMode: Text.Wrap

        font.pixelSize: 16
        color: ColorTheme.colors.light10
    }

    CheckBox
    {
        id: doNotShowAgainCheckBox

        width: parent.width
        text: qsTr("Do not show this window again")
    }

    ButtonBox
    {
        model: [
            {id: "cancel", type: Button.Type.LightInterface, text: qsTr("Cancel")},
            {id: "continue", type: Button.Type.Brand, text: control.continueButtonText}
        ]

        onClicked:
            (buttonId) =>
            {
                control.close()
                if (buttonId === "continue")
                    control.continued()
                else
                    control.cancelled()
            }
    }
}
