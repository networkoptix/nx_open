// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls

BaseSettingsScreen
{
    id: interfaceSettingsScreen

    objectName: "interfaceSettingsScreen"

    title: qsTr("Interface")

    LabeledSwitch
    {
        id: livePreviews

        width: parent.width
        text: qsTr("Live Previews")
        checkState: appContext.settings.liveVideoPreviews
            ? Qt.Checked
            : Qt.Unchecked
        extraText: qsTr("Show previews in the cameras list")
        onCheckStateChanged:
            appContext.settings.liveVideoPreviews = checkState != Qt.Unchecked
    }

    LabeledSwitch
    {
        width: parent.width
        text: qsTr("Server Time")
        checkState: appContext.settings.serverTimeMode ? Qt.Checked : Qt.Unchecked
        extraText: qsTr("Show server time on cameras")
        onCheckStateChanged:
        {
            appContext.settings.serverTimeMode = checkState != Qt.Unchecked
        }
    }
}
