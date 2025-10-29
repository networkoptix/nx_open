// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls

BaseSettingsScreen
{
    id: performanceSettingsScreen

    objectName: "performanceSettingsScreen"

    title: qsTr("Performance")

    LabeledSwitch
    {
        width: parent.width
        text: qsTr("Hardware Acceleration")
        extraText: qsTr("Can improve performance and battery life")
        checkState: appContext.settings.enableHardwareDecoding
            ? Qt.Checked
            : Qt.Unchecked
        onCheckStateChanged:
            appContext.settings.enableHardwareDecoding = checkState != Qt.Unchecked
    }

    LabeledSwitch
    {
        width: parent.width
        text: qsTr("Software Decoder Fallback")
        extraText: qsTr("Can decode rare video formats using software")
        checkState: appContext.settings.enableSoftwareDecoderFallback
            ? Qt.Checked
            : Qt.Unchecked
        onCheckStateChanged: appContext.settings.enableSoftwareDecoderFallback =
            checkState != Qt.Unchecked
    }
}
