import QtQuick 2.5;
import QtQuick.Controls 1.2;
import QtGraphicalEffects 1.0;

import "."

Item
{
    id: thisComponent;

    property Item visualParent;
    property string systemName;
    property string host;
    property bool isRecentlyConnected;

    property Component centralAreaDelegate;
    property Component expandedAreaDelegate;
    property bool correctTile: true;
    property bool isExpanded;

    Binding
    {
        target: thisComponent;
        property: "isExpanded";
        value: (tileHolder.state !== "collapsed") && correctTile;
    }

    property alias centralArea: centralAreaLoader.item;
    property alias expandedArea: expandedAreaLoader.item;

    signal connectClicked();

    function toggle()
    {
        if (!correctTile || (tileHolder.state == "moving"))
        {
            tileHolder.state = "collapsed";
            return;
        }

        var isCollapsed = (tileHolder.state == "collapsed");
        tileHolder.state = "moving";
        tileHolder.state = (isCollapsed ? "expanded" : "collapsed");
    }

    implicitWidth: 280;
    implicitHeight: 96;

    Item
    {        
        id: tileHolder;

        readonly property real expadedHeight: (loadersColumn.y
            + centralAreaLoader.height + expandedAreaLoader.height);
        width: thisComponent.width;

        onStateChanged: console.log(state, "---")
        state: "collapsed";
        states:
        [
            State
            {
                name: "collapsed";
                PropertyChanges
                {
                    target: tileHolder;

                    parent: thisComponent;
                    x: 0;
                    y: 0;
                    height: thisComponent.height;
                }
            },

            State
            {
                name: "moving";
                PropertyChanges
                {
                    target: tileHolder;

                    parent: visualParent;
                    x: thisComponent.mapToItem(visualParent, 0, 0).x;
                    y: thisComponent.mapToItem(visualParent, 0, 0).y;
                    height: thisComponent.height;
                }
            },

            State
            {
                name: "expanded";
                PropertyChanges
                {
                    target: tileHolder;

                    parent: visualParent;
                    x: (visualParent.width - tileHolder.width) / 2;
                    y: (visualParent.height - expadedHeight) / 2;
                    height: expadedHeight;
                }
            }
        ]

        transitions:
        [
            Transition
            {
                from: "expanded"; to: "moving";
                //enabled: (tileHolder.state != "collapsed");
                NumberAnimation
                {
                    properties:"x, y";
                    easing.type: Easing.InOutCubic;
                    duration: 4000;
                }

                NumberAnimation
                {
                    properties: "height";
                    easing.type: Easing.OutCubic;
                    duration: 4000;
                }
            },

            Transition
            {
                from: "moving"; to: "expanded";
                //enabled: (tileHolder.state != "collapsed");
                NumberAnimation
                {
                    properties:"x, y";
                    easing.type: Easing.InOutCubic;
                    duration: 4000;
                }

                NumberAnimation
                {
                    properties: "height";
                    easing.type: Easing.OutCubic;
                    duration: 4000;
                }
            }
        ]

        DropShadow
        {
            anchors.fill: tileArea;

            visible: thisComponent.isExpanded;
            radius: Style.custom.systemTile.shadowRadius;
            samples: Style.custom.systemTile.shadowSamples;
            color: Style.colors.shadow;
            source: tileArea;
        }

        MouseArea
        {
            id: toggleMouseArea;

            x: (thisComponent.isExpanded ? -parent.x : 0);
            y: (thisComponent.isExpanded ? -parent.y : 0);
            width: tileHolder.parent.width;
            height: tileHolder.parent.height;

            hoverEnabled: true;
            onClicked: { toggle(); }
        }

        Rectangle
        {
            id: tileArea;

            readonly property color standardColor: Style.colors.custom.systemTile.background;
            readonly property color hoveredColor: Style.lighterColor(standardColor);
            readonly property bool isHovered: (!thisComponent.isExpanded && hoverIndicator.containsMouse);

            clip: true;
            color: (isHovered || isExpanded ? hoveredColor : standardColor);

            anchors.fill: parent;
            radius: 2;

            MouseArea
            {
                id: hoverIndicator;

                anchors.fill: parent;

                acceptedButtons: (thisComponent.isExpanded
                    ? Qt.AllButtons :Qt.NoButton);
                hoverEnabled: true;
            }

            NxLabel
            {
                id: systemNameText;

                anchors.left: parent.left;
                anchors.right: parent.left;
                anchors.top: parent.top;

                anchors.leftMargin: 16;
                anchors.rightMargin: anchors.leftMargin;
                anchors.topMargin: 12;

                text: systemName;

                height: Style.custom.systemTile.systemNameLabelHeight;
                color: Style.colors.custom.systemTile.systemNameText;
                font: Style.fonts.systemTile.systemName;
            }

            Button
            {
                id: collapseTileButton;

                width: 40;
                height: 40;

                visible: thisComponent.isExpanded;
                anchors.right: parent.right;
                anchors.top: parent.top;

                text: "X";

                onClicked: { toggle(); }
            }

            Column
            {
                id: loadersColumn;

                anchors.left: parent.left;
                anchors.right: parent.right;
                anchors.top: systemNameText.bottom;

                anchors.leftMargin: 12;
                anchors.rightMargin: 16;

                Loader
                {
                    id: centralAreaLoader;

                    anchors.left: parent.left;
                    anchors.right: parent.right;

                    sourceComponent: thisComponent.centralAreaDelegate;
                }


                Loader
                {
                    id: expandedAreaLoader;

                    anchors.left: parent.left;
                    anchors.right: parent.right;

                    visible: thisComponent.isExpanded;
                    sourceComponent: thisComponent.expandedAreaDelegate;
                }
            }
        }
    }
}
