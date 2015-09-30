import QtQuick 2.1;
import QtQuick.Controls 1.1;

import "../rtu" as Rtu;
import "../base" as Base;
import "../../common" as Common;

import networkoptix.rtu 1.0 as NxRtu;

Item
{
    id: thisComponent;

    property int selectedState;
    property bool safeMode: true;
    property bool hasHdd: false;
    property bool loggedIn;
    property string serverName;
    property string information;

    signal selectionStateShouldBeChanged(int currentItemIndex);
    signal explicitSelectionCalled(int currentItemIndex);
    
    height: column.height;

    opacity: (loggedIn ? 1 : 0.3);
    
    Column
    {
        id: column;

        spacing: Common.SizeManager.spacing.base;
        anchors
        {
            left: parent.left;
            right: parent.right;
                
            leftMargin: Common.SizeManager.spacing.medium;
            rightMargin: Common.SizeManager.spacing.medium;
        }

        Item
        {
            id: bottomSpacer;
            
            width: parent.width;
            height: Common.SizeManager.spacing.small;
        }
        
        Item
        {
            id: placer;

            height: descriptionColumn.height;
            width: parent.width;

            Base.CheckBox
            {
                id: selectionCheckbox;
            
                anchors
                {
                    left: parent.left;
                    verticalCenter: parent.verticalCenter;                    
                }

                activeFocusOnPress: false;
                activeFocusOnTab: false;

                Binding
                {
                    target: selectionCheckbox;
                    property: "checkedState";
                    value: selectedState;
                }
            }
            
            Item
            {
                id: descriptionColumn;

                height: serverNameRow.height + textSpacer.height
                    + (informationText.visible ? informationText.height : 0);
                
                anchors
                {
                    left: selectionCheckbox.right;
                    right: parent.right;
                    
                    leftMargin: Common.SizeManager.spacing.medium;
                }

                Item
                {
                    id: serverNameRow;
                    height: serverNameText.height;
                    anchors
                    {
                        left: parent.left;
                        right: parent.right;
                    }

                    Base.Text
                    {
                        id: serverNameText;

                        clip: true;
                        anchors
                        {
                            left: parent.left;
                            right: (safeModeMarker.visible ? safeModeMarker.left : parent.right);
                            rightMargin: Common.SizeManager.spacing.base;
                            verticalCenter: parent.verticalCenter;
                        }

                        wrapMode: Text.Wrap;
                        text: serverName;
                        font.pixelSize: Common.SizeManager.fontSizes.base;
                    }

                    Image
                    {
                        id: safeModeMarker;

                        visible: safeMode;

                        width: height;
                        height: serverNameText.height;
                        anchors
                        {
                            verticalCenter: parent.verticalCenter;
                            right: parent.right;
                        }

                        source: "qrc:/resources/safe.png";
                    }
                }

                Item
                {
                    id: textSpacer;
                    
                    width: parent.width;
                    height: (visible ? Common.SizeManager.spacing.small : 0);
                    visible: informationText.visible;
                    
                    anchors.top: serverNameRow.bottom;
                 }

                Item
                {
                    id: informationHolder;

                    height: informationText.height;

                    anchors
                    {
                        left: parent.left;
                        right: parent.right;
                        top: textSpacer.bottom;
                    }

                    Base.Text
                    {
                        id: informationText;

                        anchors
                        {
                            left: parent.left;
                            right: (noHddMarker.visible ? noHddMarker.left : parent.right);
                        }

                        visible: (text.length !== 0);
                        text: information;
                        font.pixelSize: Common.SizeManager.fontSizes.small;
                    }

                    Image
                    {
                        id: noHddMarker;

                        visible: !hasHdd;

                        width: height;
                        height: informationText.height;

                        anchors
                        {
                            verticalCenter: parent.verticalCenter;
                            right: parent.right;
                        }

                        source: "qrc:/resources/no-hdd.png";
                    }
                }
            }
        }

        Base.LineDelimiter
        {
            color: "#e4e4e4";
        }
    }

    MouseArea
    {
        id: explicitSelectionMouseArea;

        property bool prevClicked: false;

        anchors.fill: parent;

        onClicked:
        {
            if (!thisComponent.loggedIn)
                return;

            if (prevClicked)
            {
                /// it is double click
                prevClicked = false;
                thisComponent.explicitSelectionCalled(index);
            }
            else
            {
                prevClicked = true;
                clickFilterTimer.start();
                thisComponent.selectionStateShouldBeChanged(index);
            }
        }

        onDoubleClicked:
        {
            prevClicked = false;
            clickFilterTimer.stop();
            if (thisComponent.loggedIn)
                thisComponent.explicitSelectionCalled(index);
        }

        Timer
        {
            id: clickFilterTimer;

            interval: 150;
            onTriggered: { explicitSelectionMouseArea.prevClicked = false; }
        }
    }

    Rtu.Mark
    {
        enabled: loggedIn;
        selected: (selectedState === Qt.Checked);
        anchors.fill: parent;
    }
}

