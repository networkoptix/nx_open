// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0
import QtQuick.Controls 2.4

import Nx.Core 1.0
import Nx.Ui 1.0

AbstractButton
{
    id: control

    implicitWidth: parent ? parent.width : 200
    implicitHeight: 32

    text: applicationInfo.version()
    background: null
    leftPadding: 16
    rightPadding: 16
    font.pixelSize: 13

    onDoubleClicked: copyToClipboard(versionText.text)

    onPressed: developerSettingsTimer.restart()
    onReleased: developerSettingsTimer.stop()

    contentItem: Text
    {
        id: versionText

        anchors.fill: control
        anchors.leftMargin: control.leftPadding
        anchors.rightMargin: control.rightPadding

        verticalAlignment: Text.AlignVCenter
        font.pixelSize: 13
        color: ColorTheme.colors.dark15
        text: control.text
    }

    Timer
    {
        id: developerSettingsTimer
        interval: 10000
        onTriggered: Workflow.openDeveloperSettingsScreen()
    }
}
