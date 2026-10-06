// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Core.Controls
import Nx.Controls

BaseSettingsPage
{
    id: interfaceSettingsPage

    title: qsTr("Interface")

    signal tutorialsRequested()

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

        visible: appContext.settings.iniConfigValue("enableTutorials")
        text: qsTr("Tutorials")
        showIndicator: false
        showCustomArea: true

        onClicked: interfaceSettingsPage.tutorialsRequested()

        customArea: ColoredImage
        {
            anchors.verticalCenter: parent.verticalCenter

            sourcePath: "image://skin/20x20/Outline/arrow_right.svg"
            sourceSize: Qt.size(24, 24)
            primaryColor: ColorTheme.colors.light16
        }
    }
}
