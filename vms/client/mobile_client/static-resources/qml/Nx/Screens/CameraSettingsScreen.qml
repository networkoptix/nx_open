// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Settings 1.0
import Nx.Ui 1.0

import Nx.Mobile 1.0
import nx.vms.client.core 1.0

Page
{
    id: cameraSettingsScreen

    objectName: "cameraSettingsScreen"

    property MediaPlayer player
    property alias audioSupported: audioSwitch.visible
    property AudioController audioController: null

    title: qsTr("Camera Settings")
    onLeftButtonClicked: Workflow.popCurrentScreen()
    topPadding: 4
    bottomPadding: 16

    Column
    {
        id: content

        width: parent.width
        spacing: 4

        LabeledSwitch
        {
            id: informationSwitch

            text: qsTr("Information")
            width: parent.width
            checkState: showCameraInfo ? Qt.Checked : Qt.Unchecked
            onCheckStateChanged: showCameraInfo = checkState !== Qt.Unchecked
        }

        LabeledSwitch
        {
            id: audioSwitch

            visible: audioController && audioController.audioEnabled
            text: qsTr("Audio")
            width: parent.width

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

            text: qsTr("Change Quality")
            width: parent.width
            showIndicator: false
            showCustomArea: true

            customArea: Text
            {
                text: qualityText(player.currentResolution, player.videoQuality)
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
                        settings.lastUsedQuality = dialog.activeQuality
                    }
                )
            }
        }
    }
}
