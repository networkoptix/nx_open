// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls

BaseSettingsPage
{
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
}
