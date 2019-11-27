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
    property bool collapsible: true
    property bool collapsed: false

    property alias childrenItem: control.contentItem

    width: parent.width
    contentHeight: (control.collapsed || !contentItem) ? 0 : contentItem.implicitHeight

    contentItem: AlignedColumn 
    {
        clip: true
        height: control.contentHeight
    }

    Behavior on contentHeight
    {
        id: animatedBehavior
        enabled: false

        NumberAnimation 
        { 
            duration: 200
            easing.type: Easing.InOutQuad
        }
    }

    Button
    {
        id: expandCollapseButton

        visible: control.collapsible
        parent: control.label
        anchors.right: parent.right
        height: parent.height
        width: 20

        backgroundColor: "transparent"
        hoveredColor: "transparent"
        pressedColor: "transparent"
        text: ""

        ArrowIcon
        {
            anchors.centerIn: parent
            rotation: control.collapsed ? 0 : 180
            color: expandCollapseButton.hovered ? ColorTheme.colors.light1 : ColorTheme.colors.light4
        }
        
        onClicked: 
        {
            animatedBehavior.enabled = Qt.binding(function() { return control.collapsible && control.visible })
            control.collapsed = !control.collapsed
        }
    }
}
