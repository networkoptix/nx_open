// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls
import Nx.Settings
import Nx.Ui

BaseSettingsScreen
{
    id: betaFeaturesScreen

    objectName: "betaFeaturesScreen"

    title: qsTr("Beta Features")

    LabeledSwitch
    {
        id: useDownloadVideoFeature

        width: parent.width

        text: qsTr("Video Download")
        extraText: qsTr("Ability to download video")
        checkState: appContext.settings.useDownloadVideoFeature
            ? Qt.Checked
            : Qt.Unchecked
        onCheckStateChanged:
        {
            appContext.settings.useDownloadVideoFeature =
                checkState !== Qt.Unchecked
        }
    }

    LabeledSwitch
    {
        id: useHolePunchingFeature

        width: parent.width

        text: qsTr("Speedup connections")
        extraText: qsTr("Improve network performance")
        checkState: appContext.settings.enableHolePunching
            ? Qt.Checked
            : Qt.Unchecked
        onCheckStateChanged:
        {
            const value = checkState !== Qt.Unchecked
            if (value === appContext.settings.enableHolePunching)
                return

            appContext.settings.enableHolePunching = value
            d.openRestartDialog()
        }
    }

    LabeledSwitch
    {
        id: useMaxHardwareDecodersCount

        width: parent.width

        text: qsTr("Maximum decoders count")
        extraText:
            qsTr("Improve video decoding perfomance using maximum hardware decoders count")
        checkState: appContext.settings.useMaxHardwareDecodersCount
            ? Qt.Checked
            : Qt.Unchecked
        onCheckStateChanged:
        {
            const value = checkState !== Qt.Unchecked
            if (value === appContext.settings.useMaxHardwareDecodersCount)
                return

            appContext.settings.useMaxHardwareDecodersCount = value
            d.openRestartDialog()
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
