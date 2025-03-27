// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls
import Nx.Core.Controls
import Nx.Mobile.Controls

Row
{
    id: control

    property string secretValue
    property alias backgroundMode: textInput.backgroundMode
    property alias labelText: textInput.labelText
    property alias text: textInput.text

    spacing: 8

    function resetState()
    {
        textInput.clear()
        textInput.text = secretValue
    }

    TextInput
    {
        id: textInput

        property bool maskedMode: true

        showActionButton: !control.secretValue && textInput.focus && textInput.text
        actionButtonImage.sourcePath: maskedMode
            ? "image://skin/20x20/Outline/eye_closed.svg?primary=light10"
            : "image://skin/20x20/Outline/eye_open.svg?primary=light10"
        actionButtonImage.sourceSize: Qt.size(20, 20)
        actionButtonAction: () => { maskedMode = !maskedMode }
        echoMode: control.secretValue || maskedMode
            ? TextInput.Password
            : TextInput.Normal

        width: parent.width - (clearSecretButton.visible ? clearSecretButton.width + control.spacing : 0)
        text: control.secretValue

        onActiveFocusChanged:
        {
            if (activeFocus && control.secretValue.length)
                textInput.selectAll()
        }

        Keys.onPressed:
            (event)=>
            {
                // Clear "secret" on any valuable input.
                if (event.text.length > 0 && control.secretValue.length)
                {
                    control.secretValue = ""
                    textInput.clear()
                }
            }
    }

    onSecretValueChanged: textInput.text = control.secretValue

    // TODO #ynikitenkov Add Nx.Mobile.IconButton
    Rectangle
    {
        id: clearSecretButton

        visible: control.secretValue.length
        color: textInput.background.color
        width: 56
        height: 56

        ColoredImage
        {
            anchors.centerIn: parent
            sourcePath: "image://skin/20x20/Outline/close_medium.svg?primary=light10"
            sourceSize: Qt.size(20, 20)
        }

        MouseArea
        {
            id: mouseArea
            anchors.fill: parent
            onClicked:
            {
                control.secretValue = ""
                textInput.clear()
                textInput.forceActiveFocus()
            }
        }

        MaterialEffect
        {
            anchors.fill: parent
            rounded: true
            mouseArea: mouseArea
            rippleColor: "transparent"
        }
    }
}
