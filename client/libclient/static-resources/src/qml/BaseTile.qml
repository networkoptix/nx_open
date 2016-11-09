import QtQuick 2.5;
import QtQuick.Controls 1.2;
import QtGraphicalEffects 1.0;
import Qt.labs.controls 1.0;

import "."

Item
{
    id: control;

    property Item visualParent;

    property alias tileColor: tileArea.color;
    property bool isHovered: (hoverIndicator.containsMouse ||
        (expandedOpacity == 1) || menuButton.isHovered) && !isConnecting;

    property alias titleLabel: systemNameLabel;
    property alias collapseButton: collapseTileButton;
    property alias menuButton: menuButtonControl;
    property alias indicator: indicatorControl;

    property alias areaLoader: areaLoader;

    property bool isConnecting: false;
    property bool isExpanded: false;
    property bool isAvailable: false;
    property real expandedOpacity: shadow.opacity;
    property bool isOnline: false;
    property bool isCloudTile: false;
    property string systemId;
    property string localId;
    property bool animating: false;

    signal collapsedTileClicked();

    property bool forceImmediateAnimation: false;

    function getDuration(duration)
    {
        return (forceImmediateAnimation ? 0 : duration);
    }

    function forceCollapsedState()
    {
        control.forceImmediateAnimation = true;
        tileHolder.state = "collapsed";
        control.forceImmediateAnimation = false;
    }

    function toggle()
    {
        tileHolder.state = (isExpanded ? "collapsed" : "expanded");
    }

    implicitWidth: 280;
    implicitHeight: 96;
    z: (transition.running ? 100 : 0)

    Item
    {
        id: tileHolder;

        readonly property real expadedHeight: (areaLoader.y + areaLoader.height);
        width: control.width;

        state: "collapsed";

        states:
        [
            State
            {
                name: "collapsed";

                ParentChange
                {
                    target: tileHolder;
                    parent: control;
                    x: 0;
                    y: 0;
                    height: control.height;
                }

                PropertyChanges
                {
                    target: collapseTileButton;
                    opacity: 0;
                }

                PropertyChanges
                {
                    target: hideTileButton;
                    opacity: (control.isOnline ? 0 : 1);
                }

                PropertyChanges
                {
                    target: shadow;
                    opacity: 0;
                }

                PropertyChanges
                {
                    target: indicator;
                    opacity: 1;
                }

                PropertyChanges
                {
                    target: menuButtonControl;
                    opacity: 1;
                }
            },

            State
            {
                name: "expanded";

                ParentChange
                {
                    target: tileHolder;
                    parent: visualParent;
                    x: (visualParent.width - tileHolder.width) / 2;
                    y: (visualParent.height - expadedHeight) / 2;
                    height: expadedHeight;
                }

                PropertyChanges
                {
                    target: collapseTileButton;
                    opacity: 1;
                }

                PropertyChanges
                {
                    target: hideTileButton;
                    opacity: 0;
                }


                PropertyChanges
                {
                    target: shadow;
                    opacity: 1;
                }

                PropertyChanges
                {
                    target: indicator;
                    opacity: 0;
                }

                PropertyChanges
                {
                    target: menuButtonControl;
                    opacity: 0;
                }
            }
        ]

        transitions: Transition
        {
            id: transition;

            SequentialAnimation
            {
                id: anim;

                PropertyAction
                {
                    target: control;
                    properties: "isExpanded, animating";
                    value: true;
                }

                ParallelAnimation
                {
                    ParentAnimation
                    {
                        via: control.visualParent;

                        NumberAnimation
                        {
                            properties: "x, y";
                            easing.type: Easing.InOutCubic;
                            duration: control.getDuration(400);
                        }

                        NumberAnimation
                        {
                            properties: "height";
                            easing.type: Easing.OutCubic;
                            duration: control.getDuration(400);
                        }
                    }

                    NumberAnimation
                    {
                        targets: [collapseTileButton, hideTileButton, menuButtonControl];
                        properties: "opacity";
                        easing.type: Easing.OutCubic;
                        duration: control.getDuration(200);
                    }

                    NumberAnimation
                    {
                        targets: [shadow];
                        properties: "opacity";
                        easing.type: Easing.OutCubic;
                        duration: control.getDuration(400);
                    }

                    NumberAnimation
                    {
                        target: indicator;
                        properties: "opacity";
                        easing.type: (tileHolder.state == "collapsed" ?
                              Easing.InCubic : Easing.OutCubic);
                        duration: control.getDuration(200);
                    }
                }

                PropertyAction
                {
                    target: control;
                    property: "isExpanded";
                    value: (tileHolder.state == "expanded");
                }

                PropertyAction
                {
                    target: control;
                    property: "animating";
                    value: false;
                }
            }
        }

        DropShadow
        {
            id: shadow;

            anchors.fill: tileArea;

            visible: control.isExpanded;
            radius: Style.custom.systemTile.shadowRadius;
            samples: Style.custom.systemTile.shadowSamples;
            color: Style.colors.shadow;
            source: tileArea;
        }

        MouseArea
        {
            id: toggleMouseArea;

            x: (control.isExpanded ? -parent.x : 0);
            y: (control.isExpanded ? -parent.y : 0);
            width: tileHolder.parent.width;
            height: tileHolder.parent.height;

            hoverEnabled: true;
            onPressed:
            {
                if (context.connectingToSystem.length)
                    return;

                if (control.isExpanded)
                {
                    control.toggle();
                    mouse.accepted = false;
                }
            }

            onReleased:
            {
                if (context.connectingToSystem.length)
                    return;

                if (!control.isExpanded)
                    control.collapsedTileClicked();
            }
        }

        Rectangle
        {
            id: tileArea;

            enabled: control.enabled;

            clip: true;
            color: Style.colors.custom.systemTile.background;
            anchors.fill: parent;
            radius: 2;

            Item
            {
                anchors.fill: parent;

                opacity: (control.isConnecting && !isExpanded ? 0.2 : 1);

                MouseArea
                {
                    id: hoverIndicator;

                    anchors.fill: parent;
                    acceptedButtons: (control.isExpanded ? Qt.AllButtons :Qt.NoButton);
                    hoverEnabled: true;
                }

                NxLabel
                {
                    id: systemNameLabel;

                    disableable: false;
                    anchors.left: parent.left;
                    anchors.right: (collapseTileButton.visible ? collapseTileButton.left :
                        (menuButtonControl.visible ? menuButtonControl.left : parent.right));
                    anchors.top: parent.top;

                    anchors.leftMargin: 16;
                    anchors.rightMargin: (collapseTileButton.visible || menuButtonControl.visible ?
                        0 : anchors.leftMargin);
                    anchors.topMargin: 12;

                    elide: Text.ElideRight;

                    height: Style.custom.systemTile.systemNameLabelHeight;
                    color: (control.isAvailable ? Style.colors.text : Style.colors.midlight);
                    font: Style.fonts.systemTile.systemName;
                }

                NxMenuButton
                {
                    id: menuButtonControl;

                    width: 24;
                    height: 24;

                    anchors.right: parent.right;
                    anchors.top: parent.top;

                    anchors.rightMargin: 6;
                    anchors.topMargin: 12;
                }

                NxButton
                {
                    id: collapseTileButton;

                    width: 40;
                    height: 40;

                    iconUrl: "qrc:/skin/welcome_page/close.png";
                    visible: (opacity != 0) && isExpanded;
                    anchors.right: parent.right;
                    anchors.top: parent.top;

                    bkgColor: tileArea.color;
                    hoveredColor: Style.colors.custom.systemTile.closeButtonBkg;
                    enabled: !control.isConnecting;

                    onClicked: { control.toggle(); }
                }

                NxButton
                {
                    id: hideTileButton;

                    width: 24;
                    height: 24;

                    visible: (opacity != 0);
                    anchors.right: parent.right;
                    anchors.top: parent.top;

                    anchors.rightMargin: 6;
                    anchors.topMargin: 12;

                    iconUrl: "qrc:/skin/welcome_page/tile_hide.png";
                    hoveredIconUrl: "qrc:/skin/welcome_page/tile_hide_hovered.png";
                    pressedIconUrl: "qrc:/skin/welcome_page/tile_hide_pressed.png";
                    showBackground: false;

                    bkgColor: tileArea.color;
                    onClicked:
                    {
                        var id = (isCloudTile ? control.systemId : control.localId);
                        context.hideSystem(id);
                    }
                }

                Loader
                {
                    id: areaLoader;

                    anchors.left: parent.left;
                    anchors.right: parent.right;
                    anchors.top: systemNameLabel.bottom;

                    anchors.leftMargin: 12;
                    anchors.rightMargin: 16;

                    sourceComponent: control.centralAreaDelegate;
                }

                Indicator
                {
                    id: indicatorControl;

                    visible: false;

                    anchors.right: parent.right;
                    anchors.top: parent.top;
                    anchors.rightMargin: 14;
                    anchors.topMargin: 66;
                }
            }

            NxDotPreloader
            {
                visible: control.isConnecting && !control.isExpanded;
                anchors.centerIn: parent;
                color: Style.colors.text;
            }

        }
    }
}
