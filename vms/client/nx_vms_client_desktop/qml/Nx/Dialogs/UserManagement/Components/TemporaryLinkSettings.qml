// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Core
import Nx.Controls

import nx.vms.client.desktop

Column
{
    id: control

    spacing: 8

    width: parent.width

    property alias linkValidUntil: datePicker.currentDate
    property alias expiresAfterLoginS: expiresAfterSpinBox.seconds
    property alias revokeAccessEnabled: revokeAccessCheckBox.checked
    property var self

    property alias leftSideMargin: validUntilField.leftSideMargin
    property alias rightSideMargin: validUntilField.rightSideMargin

    CenteredField
    {
        id: validUntilField

        text: qsTr("Link Valid Until")

        hint: ContextHintButton
        {
            toolTipText: qsTr("The link will remain accessible until the date specified"
             + " (including, based on server time)")
        }

        DatePicker
        {
            id: datePicker

            enabled: control.enabled
            displayOffset: (time) => { return control.self ? control.self.displayOffset(time) : 0 }
        }
    }

    CenteredField
    {
        leftSideMargin: validUntilField.leftSideMargin
        rightSideMargin: validUntilField.rightSideMargin

        RowLayout
        {
            spacing: 2

            anchors
            {
                left: parent.left
                right: parent.right
                leftMargin: -3
            }

            CheckBox
            {
                id: revokeAccessCheckBox

                text: qsTr("Revoke access after login")

                font.pixelSize: 14
                wrapMode: Text.WordWrap
                enabled: control.enabled
            }

            Item
            {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop

                ContextHintButton
                {
                    toolTipText: qsTr("Access may be revoked earlier if the link validity"
                        + " period ends")
                }
            }
        }
    }

    CenteredField
    {
        leftSideMargin: validUntilField.leftSideMargin
        rightSideMargin: validUntilField.rightSideMargin

        enabled: revokeAccessCheckBox.checked

        //: 'In' as in sentence: 'Revoke access after login in N hours'.
        text: qsTr("In")

        TimeDuration
        {
            id: expiresAfterSpinBox

            enabled: control.enabled

            suffixes: [
                TimeDurationSuffixModel.Suffix.minutes,
                TimeDurationSuffixModel.Suffix.hours,
                TimeDurationSuffixModel.Suffix.days,
            ]

            seconds: 8 * 60 * 60 //< 8h.

            valueFrom: 1
            valueTo: 999
        }
    }
}
