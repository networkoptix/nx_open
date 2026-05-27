// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls

BaseSettingsPage
{
    title: qsTr("Interface")

    LabeledSwitch
    {
        id: livePreviews

        width: parent.width
        text: qsTr("Live Previews")
        extraText: qsTr("Show previews in the cameras list")

        checkState: appContext.settings.liveVideoPreviews
            ? Qt.Checked
            : Qt.Unchecked

        onCheckStateChanged:
            appContext.settings.liveVideoPreviews = checkState != Qt.Unchecked
    }

    LabeledSwitch
    {
        width: parent.width
        text: qsTr("Server Time")
        extraText: qsTr("Allows to show server time for the camera")
        checkState: appContext.settings.serverTimeMode ? Qt.Checked : Qt.Unchecked

        onCheckStateChanged:
            appContext.settings.serverTimeMode = checkState != Qt.Unchecked
    }

    LabeledSwitch
    {
        width: parent.width
        text: qsTr("Mirror Timeline")
        extraText: qsTr("Flip the timeline for left-handed use")
        checkState: appContext.settings.leftHandedMode ? Qt.Checked : Qt.Unchecked

        onCheckStateChanged:
            appContext.settings.leftHandedMode = checkState != Qt.Unchecked
    }
}
