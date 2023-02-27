// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0
import QtQml 2.14
import QtQuick.Layouts 1.0

import Nx.Controls 1.0
import Nx 1.0

import "private"

Group
{
    id: control

    property string name: ""
    property alias caption: panel.title
    property string description: ""
    property bool collapsible: false
    property bool collapsed: false

    property bool fillWidth: true

    childrenItem: panel.contentItem

    contentItem: Panel
    {
        id: panel

        contentHeight: column.implicitHeight
        clip: true

        topPadding: 36
        bottomPadding: 16

        contentItem: LabeledColumnLayout
        {
            id: column

            layoutControl: control
        }

        states: State
        {
            name: "collapsed"
            when: control.collapsed
            PropertyChanges { target: panel; contentHeight: 0 }
        }

        transitions: Transition
        {
            NumberAnimation
            {
                properties: "contentHeight"
                easing.type: Easing.InOutQuad
                duration: 200
            }
        }
    }

    MouseArea
    {
        id: expandCollapseButton
        Layout.fillHeight: true

        visible: control.collapsible
        parent: panel.label
        width: 20
        height: parent.height
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true

        ArrowIcon
        {
            anchors.centerIn: parent
            rotation: control.collapsed ? 0 : 180
            color: expandCollapseButton.containsMouse && !expandCollapseButton.pressed
                ? ColorTheme.colors.light1
                : ColorTheme.colors.light4
        }

        onClicked:
            control.collapsed = !control.collapsed
    }
}
