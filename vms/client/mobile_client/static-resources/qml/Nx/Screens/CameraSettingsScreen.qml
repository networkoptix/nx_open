// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Core
import Nx.Controls
import Nx.Items
import Nx.Settings
import Nx.Ui

import Nx.Mobile
import nx.vms.client.core

AdaptiveScreen
{
    id: cameraSettingsScreen

    objectName: "cameraSettingsScreen"

    property MediaPlayer player
    property alias audioSupported: audioSwitch.visible
    property AudioController audioController: null

    title: qsTr("Camera Settings")

    contentItem: Item
    {
        ColumnLayout
        {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 20

            spacing: 4

            LabeledSwitch
            {
                id: audioSwitch

                Layout.fillWidth: true

                visible: audioController && audioController.audioEnabled
                text: qsTr("Audio")

                checkState: audioController && audioController.audioEnabled
                    ? Qt.Checked
                    : Qt.Unchecked

                onCheckStateChanged:
                {
                    if (audioController)
                        audioController.audioEnabled = checkState !== Qt.Unchecked
                }
            }

            LabeledSwitch
            {
                id: changeQualitySwitch

                Layout.fillWidth: true

                text: qsTr("Change Quality")
                showIndicator: false
                showCustomArea: true

                customArea: Text
                {
                    text:
                    {
                        const resolution = player.currentResolution
                        if (resolution.width > 0 && resolution.height > 0)
                            return ("%1x%2").arg(resolution.width).arg(resolution.height);

                        if (player.videoQuality !== MediaPlayer.LowVideoQuality
                            && player.videoQuality !== MediaPlayer.HighVideoQuality)
                        {
                            return ("%1p").arg(player.videoQuality);
                        }
                        return qsTr("Unknown", "Unknown video quality");
                    }
                    font.pixelSize: 16
                    color: ColorTheme.colors.light16
                    anchors.verticalCenter: parent.verticalCenter
                }

                customAreaClickHandler: function()
                {
                    var customQualities = [ 1080, 720, 480, 360 ]
                    var allVideoQualities =
                        [ MediaPlayer.LowVideoQuality, MediaPlayer.HighVideoQuality ]
                            .concat(customQualities)

                    var actualQuality = player.actualVideoQuality()
                    if (actualQuality === MediaPlayer.CustomVideoQuality)
                        actualQuality = player.currentResolution.height

                    var dialog = Workflow.openDialog(
                        "../Screens/private/VideoScreen/QualityDialog.qml",
                        {
                            "actualQuality": player.currentResolution,
                            "activeQuality": actualQuality,
                            "customQualities": customQualities,
                            "availableVideoQualities":
                                player.availableVideoQualities(allVideoQualities),
                            "transcodingSupportStatus":
                                player.transcodingStatus()
                        }
                    )

                    dialog.onActiveQualityChanged.connect(
                        function()
                        {
                            appContext.settings.lastUsedQuality = dialog.activeQuality
                        }
                    )
                }
            }
        }
    }
}
