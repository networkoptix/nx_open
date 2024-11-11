// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0
import QtQuick.Controls 2.4
import Nx.Core 1.0

ToolBar
{
    id: toolBar

    property bool useGradientBackground: false
    property real statusBarHeight: deviceStatusBarHeight

    signal clicked()

    implicitHeight: 56
    contentItem.clip: true

    width: parent
        ? parent.width
        : 200

    background: Loader
    {
        sourceComponent: useGradientBackground
            ? gradientBackground
            : standardBackground
        y: -toolBar.statusBarHeight
        x: -mainWindow.leftPadding
        width: mainWindow.width + mainWindow.leftPadding
        height: parent.height + toolBar.statusBarHeight
    }

    Component
    {
        id: standardBackground

        Rectangle
        {
            color: ColorTheme.colors.windowBackground

            Rectangle
            {
                width: parent.width
                height: statusBarHeight
                anchors.bottom: parent.top
                color: parent.color
            }

            Rectangle
            {
                width: parent.width
                height: 1
                anchors.top: parent.bottom
                color: ColorTheme.colors.dark4
            }

            MouseArea
            {
                anchors.fill: parent
                onClicked: toolBar.clicked()
            }
        }
    }

    Component
    {
        id: gradientBackground

        Image
        {
            source: lp("/images/toolbar_gradient.png")
        }
    }
}
