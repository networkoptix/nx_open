import QtQuick 2.5;
import QtQuick.Controls 1.2;
import QtGraphicalEffects 1.0;

import "."

Item
{
    id: thisComponent;

    property Item visualParent;
    property string systemName;
    property bool isRecentlyConnected;

    property Component centralAreaDelegate;
    property Component expandedAreaDelegate;
    property bool allowExpanding;

    property string notAvailableLabelText: "";

    property bool isAvailable: true;
    property bool isExpandable: true;
    property bool isOnline: false;
    property bool isValidVersion: true;
    property bool isValidCustomization: true;

    property bool isExpanded;

    property color standardColor: Style.colors.custom.systemTile.background;
    property color hoveredColor: Style.lighterColor(standardColor);
    property color inactiveColor: Style.colors.shadow;
    property alias collapseButton: collapseTileButton;
    property Item collapseButtonTabItem: null;

    Binding
    {
        target: thisComponent;
        property: "isExpanded";
        value: (tileHolder.state !== "collapsed") && allowExpanding;
    }
    Binding
    {
        target: thisComponent;
        property: "allowExpanding";
        value: isExpandable && isAvailable;
    }

    property alias centralArea: centralAreaLoader.item;
    property alias expandedArea: expandedAreaLoader.item;

    signal connectClicked();

    function toggle()
    {
        tileHolder.state = (!allowExpanding || (tileHolder.state == "expanded")
            ? "collapsed" : "expanded");
    }

    implicitWidth: 280;
    implicitHeight: 96;
    z: (transition.running ? 100 : 0)

    FocusScope
    {
        id: tileHolder;

        readonly property real expadedHeight: (loadersColumn.y
            + centralAreaLoader.height + expandedAreaLoader.height);
        width: thisComponent.width;

        state: "collapsed";
        states:
        [
            State
            {
                name: "collapsed";

                ParentChange
                {
                    target: tileHolder;
                    parent: thisComponent;
                    x: 0;
                    y: 0;
                    height: thisComponent.height;
                }

                PropertyChanges
                {
                    target: collapseTileButton;
                    opacity: 0;
                }

                PropertyChanges
                {
                    target: shadow;
                    opacity: 0;
                }

                PropertyChanges
                {
                    target: expandedAreaLoader;
                    opacity: 0;
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
                    target: shadow;
                    opacity: 1;
                }

                PropertyChanges
                {
                    target: expandedAreaLoader;
                    opacity: 1;
                }
            }
        ]

        transitions: Transition
        {
            id: transition;

            ParentAnimation
            {
                NumberAnimation
                {
                    properties: "x, y";
                    easing.type: Easing.InOutCubic;
                    duration: 400;
                }

                NumberAnimation
                {
                    properties: "height";
                    easing.type: Easing.OutCubic;
                    duration: 400;
                }
            }
            NumberAnimation
            {
                target: collapseTileButton;
                properties: "opacity";
                easing.type: Easing.OutCubic;
                duration: 200;
            }

            NumberAnimation
            {
                target: shadow;
                properties: "opacity";
                easing.type: Easing.OutCubic;
                duration: 400;
            }

            NumberAnimation
            {
                target: expandedAreaLoader;
                properties: "opacity";
                easing.type: Easing.OutCubic;
                duration: 400;
            }
        }

        DropShadow
        {
            id: shadow;

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
            onClicked:
            {
                if (thisComponent.enabled)
                    thisComponent.toggle();
            }
        }

        Rectangle
        {
            id: tileArea;

            enabled: thisComponent.enabled;

            readonly property bool isHovered: (!thisComponent.isExpanded && hoverIndicator.containsMouse);

            clip: true;
            color:
            {
                if (!thisComponent.isAvailable)
                    return thisComponent.inactiveColor;
                if (isHovered || thisComponent.isExpanded)
                    return thisComponent.hoveredColor;

                return thisComponent.standardColor;
            }

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

                disableable: false;
                anchors.left: parent.left;
                anchors.right: parent.left;
                anchors.top: parent.top;

                anchors.leftMargin: 16;
                anchors.rightMargin: anchors.leftMargin;
                anchors.topMargin: 12;

                text: systemName;

                height: Style.custom.systemTile.systemNameLabelHeight;
                color: (thisComponent.isAvailable ? Style.colors.text
                    : Style.colors.midlight);
                font: Style.fonts.systemTile.systemName;
            }

            NxButton
            {
                id: collapseTileButton;

                width: 40;
                height: 40;

                visible: (opacity != 0);
                anchors.right: parent.right;
                anchors.top: parent.top;

                iconUrl: "qrc:/skin/welcome_page/close.png";
                bkgColor: tileArea.color;
                hoveredColor: Style.colors.custom.systemTile.closeButtonBkg;

                onClicked: { thisComponent.toggle(); }
                KeyNavigation.tab: thisComponent.collapseButtonTabItem;
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

                    visible: (opacity != 0);
                    sourceComponent: thisComponent.expandedAreaDelegate;
                }
            }

            Indicator
            {
                id: errorLabel;

                property bool isErrorIndicator: (!thisComponent.isValidCustomization
                    || !thisComponent.isValidVersion);

                anchors.right: parent.right;
                anchors.bottom: parent.bottom;
                anchors.rightMargin: 14;
                anchors.bottomMargin: anchors.rightMargin;

                visible: (!thisComponent.isAvailable
                    && thisComponent.notAvailableLabelText.length);

                text: thisComponent.notAvailableLabelText;
                textColor: (isErrorIndicator ? Style.colors.shadow : Style.colors.windowText);
                color: (isErrorIndicator ? Style.colors.red_main
                    : Style.colors.custom.systemTile.offlineIndicatorBkg);
            }
        }
    }
}
