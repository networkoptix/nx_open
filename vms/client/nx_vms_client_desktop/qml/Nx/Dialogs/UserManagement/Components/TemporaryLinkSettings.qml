// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx
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
    property alias displayOffsetMs: datePicker.displayOffset

    property alias leftSideMargin: validUntilField.leftSideMargin
    property alias rightSideMargin: validUntilField.rightSideMargin

    CenteredField
    {
        id: validUntilField

        text: qsTr("Link Valid until")

        DatePicker
        {
            id: datePicker

            enabled: control.enabled
        }
    }

    CenteredField
    {
        leftSideMargin: validUntilField.leftSideMargin
        rightSideMargin: validUntilField.rightSideMargin

        CheckBox
        {
            id: revokeAccessCheckBox

            text: qsTr("Revoke access after login")

            font.pixelSize: 14
            wrapMode: Text.WordWrap
            enabled: control.enabled

            anchors
            {
                left: parent.left
                right: parent.right
                leftMargin: -3
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
