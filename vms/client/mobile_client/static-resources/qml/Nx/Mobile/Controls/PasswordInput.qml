// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls
import Nx.Core
import Nx.Core.Controls
import Nx.Mobile.Controls

FocusScope
{
    id: control

    property bool hasSecretValue: true
    property alias backgroundMode: textInput.backgroundMode
    property alias labelText: textInput.labelText
    property alias text: textInput.text
    property alias supportText: textInput.supportText
    property alias errorText: textInput.errorText

    function resetState(hasSecretValue)
    {
        control.hasSecretValue = hasSecretValue
        textInput.text = hasSecretValue
            ? d.kMaskText
            : ""
    }

    implicitWidth: row.width
    implicitHeight: row.height

    signal accepted()
    signal secretIsCleared()

    Row
    {
        id: row

        spacing: 8

        TextInput
        {
            id: textInput

            property bool maskedMode: true

            focus: true
            enabled: !control.hasSecretValue
            passwordCharacter: "*"
            showActionButton: !control.hasSecretValue && textInput.focus && textInput.text
            actionButtonImage.sourcePath: maskedMode
                ? "image://skin/20x20/Outline/eye_closed.svg?primary=light10"
                : "image://skin/20x20/Outline/eye_open.svg?primary=light10"
            actionButtonImage.sourceSize: Qt.size(20, 20)
            actionButtonAction: () => { maskedMode = !maskedMode }
            inputMethodHints: Qt.ImhSensitiveData | Qt.ImhPreferLatin | Qt.ImhNoPredictiveText
                | Qt.ImhNoAutoUppercase
            echoMode: control.hasSecretValue || maskedMode
                ? TextInput.Password
                : TextInput.Normal
            text: d.kMaskText
            width: control.width
                - (clearSecretButton.visible ? clearSecretButton.width + row.spacing : 0)

            onActiveFocusChanged:
            {
                if (activeFocus && !control.hasSecretValue)
                    textInput.selectAll()
            }

            onAccepted: control.accepted()
        }

        // TODO #ynikitenkov Add Nx.Mobile.IconButton
        Rectangle
        {
            id: clearSecretButton

            visible: control.hasSecretValue
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
                    control.resetState(/*hasSecretValue*/ false)
                    textInput.errorText = ""
                    control.secretIsCleared()
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

        NxObject
        {
            id: d

            readonly property string kMaskText: "*************"
        }
    }
}
