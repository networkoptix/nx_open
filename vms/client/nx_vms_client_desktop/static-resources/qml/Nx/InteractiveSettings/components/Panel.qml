import QtQuick 2.0

import Nx.Controls 1.0
import Nx 1.0

import "private"

Panel
{
    id: control

    property string name: ""
    property alias caption: control.title
    property string description: ""
    property bool collapsible: false
    property bool collapsed: false

    property alias childrenItem: control.contentItem

    width: parent.width
    contentHeight: contentItem ? contentItem.implicitHeight : 0

    contentItem: AlignedColumn
    {
        id: column

        clip: true
        height: control.contentHeight

        Binding
        {
            target: column
            property: "labelWidthHint"
            value: control.parent.labelWidth - control.x - x
            when: control.parent && control.parent.hasOwnProperty("labelWidth")
        }
    }

    states: State
    {
        name: "collapsed"
        when: control.collapsed
        PropertyChanges { target: control; contentHeight: 0 }
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

    MouseArea
    {
        id: expandCollapseButton

        visible: control.collapsible
        parent: control.label
        anchors.right: parent.right
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
