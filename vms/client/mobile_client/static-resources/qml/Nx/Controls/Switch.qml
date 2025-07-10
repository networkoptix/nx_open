// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx.Core 1.0

import "private"

Control
{
    id: control

    property alias checkState: indicator.checkState
    property alias animationDuration: indicator.animationDuration
    property bool toggleOnClick: true

    signal clicked()

    function toggle()
    {
        indicator.toggle()
    }

    padding: 6
    leftPadding: 8
    rightPadding: 8

    implicitWidth: indicator.implicitWidth + leftPadding + rightPadding
    implicitHeight: indicator.implicitHeight + topPadding + bottomPadding

    background: null

    contentItem: SwitchIndicator
    {
        id: indicator
    }

    MouseArea
    {
        anchors.fill: parent
        onClicked:
        {
            if (control.toggleOnClick)
                indicator.toggle()

            control.clicked()
        }
    }
}
