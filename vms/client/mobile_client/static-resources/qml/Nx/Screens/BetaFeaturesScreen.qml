// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls
import Nx.Settings
import Nx.Ui

Page
{
    id: betaFeaturesScreen

    objectName: "betaFeaturesScreen"

    title: qsTr("Beta Features")
    onLeftButtonClicked: Workflow.popCurrentScreen()

    Flickable
    {
        id: flickable

        anchors.fill: parent

        contentWidth: width
        contentHeight: content.height

        Column
        {
            id: content

            width: flickable.width
            spacing: 4

            LabeledSwitch
            {
                id: useDownloadVideoFeature

                width: parent.width

                icon: lp("/images/download_setting.svg")
                text: qsTr("Video Download")
                extraText: qsTr("Ability to download video")
                checkState: settings.useVideoDownloadFeature ? Qt.Checked : Qt.Unchecked
                onCheckStateChanged: settings.useVideoDownloadFeature = checkState !== Qt.Unchecked
            }

            LabeledSwitch
            {
                id: useHolePunchingFeature

                width: parent.width

                icon: lp("/images/speedup_connections.svg")
                text: qsTr("Speedup connections")
                extraText: qsTr("Improve network performance")
                checkState: settings.enableHolePunching ? Qt.Checked : Qt.Unchecked
                onCheckStateChanged:
                {
                    const value = checkState !== Qt.Unchecked
                    if (value === settings.enableHolePunching)
                        return

                    settings.enableHolePunching = value
                    d.openRestartDialog()
                }
            }

            LabeledSwitch
            {
                id: useMaxHardwareDecodersCount

                width: parent.width

                icon: lp("/images/max_hardware_decoders_count.svg")
                text: qsTr("Maximum decoders count")
                extraText:
                    qsTr("Improve video decoding perfomance using maximum hardware decoders count")
                checkState: settings.useMaxHardwareDecodersCount ? Qt.Checked : Qt.Unchecked
                onCheckStateChanged:
                {
                    const value = checkState !== Qt.Unchecked
                    if (value === settings.useMaxHardwareDecodersCount)
                        return

                    settings.useMaxHardwareDecodersCount = value
                    d.openRestartDialog()
                }
            }
        }
    }

    QtObject
    {
        id: d

        function openRestartDialog()
        {
            Workflow.openStandardDialog(
                qsTr("Please restart the app to apply the changes."))
        }
    }
}
