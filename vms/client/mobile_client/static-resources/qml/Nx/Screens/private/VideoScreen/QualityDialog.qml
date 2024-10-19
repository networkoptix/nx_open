// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import Nx.Dialogs
import Nx.Core

import nx.vms.client.core

DialogBase
{
    id: qualityDialog

    property int activeQuality: MediaPlayer.LowVideoQuality
    property size actualQuality
    property var customQualities: []
    property var availableVideoQualities: []
    property int transcodingSupportStatus: MediaPlayer.TranscodingSupported

    Column
    {
        width: parent.width

        DialogTitle
        {
            text: qsTr("Video Quality")
            bottomPadding: 6
        }
        DialogMessage
        {
            text:
            {
                if (actualQuality.width > 0 && actualQuality.height > 0)
                    return actualQuality.width + " x " + actualQuality.height

                if (activeQuality !== MediaPlayer.LowVideoQuality
                    && activeQuality !== MediaPlayer.HighVideoQuality)
                {
                    return activeQuality + 'p'
                }

                return qsTr("Unknown")
            }
            font.pixelSize: 14
            bottomPadding: 12
        }

        DialogSeparator {}

        QualityItem
        {
            quality: MediaPlayer.LowVideoQuality
            text: qsTr("Highest speed")
        }
        QualityItem
        {
            quality: MediaPlayer.HighVideoQuality
            text: qsTr("Best quality")
        }

        DialogSeparator {}

        Text
        {
            width: parent.width
            topPadding: 8
            bottomPadding: 8
            leftPadding: 16
            rightPadding: 16

            // Workaround for a Qt bug: Text item does not take paddings into account when
            // calculates its implicit size.
            height: contentHeight + topPadding + bottomPadding

            font.pixelSize: 13
            color: ColorTheme.colors.red_core

            text:
            {
                if (transcodingSupportStatus === MediaPlayer.TranscodingSupported)
                    return ""

                if (transcodingSupportStatus === MediaPlayer.TranscodingDisabled)
                    return qsTr("Transcoding is disabled by the Server.")
                if (transcodingSupportStatus === MediaPlayer.TranscodingNotSupported)
                    return qsTr("Transcoding is not supported for this camera.")
                if (transcodingSupportStatus === MediaPlayer.TranscodingNotSupportedForServersOlder30)
                    return qsTr("Transcoding is not supported for Servers with version lower than 3.0.")

                return ""
            }
            wrapMode: Text.WordWrap
            visible: text !== ""
        }

        Repeater
        {
            model: customQualities
            QualityItem { quality: modelData }
        }
    }
}
