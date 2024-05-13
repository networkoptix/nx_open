// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx
import Nx.Core

import nx.vms.common

Item
{
    property bool cameraHasArchive: false
    property bool anyCameraHasArchive: false
    property int timePeriodType: CommonGlobals.RecordingContent

    anchors
    {
        bottom: parent ? parent.bottom : undefined
        bottomMargin: 6
        horizontalCenter: parent ? parent.horizontalCenter : undefined
    }

    width: 16
    height: 2

    Rectangle
    {
        anchors.fill: parent
        color: {
            switch (timePeriodType)
            {
                case CommonGlobals.RecordingContent:
                    return cameraHasArchive ? ColorTheme.colors.green_l : ColorTheme.colors.green_d
                case CommonGlobals.MotionContent:
                    return cameraHasArchive ? ColorTheme.colors.red_l : ColorTheme.colors.red_d
                case CommonGlobals.AnalyticsContent:
                    return cameraHasArchive
                        ? ColorTheme.colors.yellow_l : ColorTheme.colors.yellow_d
            }
        }
        radius: height / 2
        visible: cameraHasArchive || anyCameraHasArchive
    }
}
