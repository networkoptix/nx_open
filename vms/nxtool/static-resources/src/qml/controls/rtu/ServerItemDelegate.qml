import QtQuick 2.1;
import QtQuick.Controls 1.1;

import "../rtu" as Rtu;
import "../base" as Base;
import "../../common" as Common;

import networkoptix.rtu 1.0 as NxRtu;

Rectangle
{
    id: thisComponent;

    property int port;
    property int selectedState;

    property bool loggedIn: false;
    property bool availableForSelection: false;
    property bool safeMode: true;
    property bool hasHdd: false;
    property bool availableByHttp: false;

    property string serverName;
    property string version;
    property string operation;
    property string displayAddress;
    property string hardwareAddress;
    property string os;

    signal unauthrizedClicked(int currentItemIndex);
    signal selectionStateShouldBeChanged(int currentItemIndex);
    signal explicitSelectionCalled(int currentItemIndex);
    
    height: column.height;

    color: (availableForSelection ? "white" : "#F0F0F0");
    
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
            id: holder;

            width: parent.width;
            height: infoHolder.height;

            Base.CheckBox
            {
                id: selectionCheckbox;

                enabled: thisComponent.availableForSelection;

                anchors
                {
                    left: parent.left;
                    top: parent.top;
                    bottom: parent.bottom;
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

            MouseArea
            {
                id: explicitSelectionMouseArea;

                property bool prevClicked: false;

                anchors.fill: parent;

                onClicked:
                {
                    if (!thisComponent.loggedIn)
                    {
                        unauthrizedClicked(index);
                        return;
                    }

                    if (!thisComponent.availableForSelection)
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
                    if (thisComponent.availableForSelection)
                        thisComponent.explicitSelectionCalled(index);
                }

                Timer
                {
                    id: clickFilterTimer;

                    interval: 150;
                    onTriggered: { explicitSelectionMouseArea.prevClicked = false; }
                }
            }

            Column
            {
                id: infoHolder;

                anchors
                {
                    left: selectionCheckbox.right;
                    leftMargin: Common.SizeManager.spacing.medium;

                    right: parent.right;
                }

                spacing: Common.SizeManager.spacing.base;

                Item
                {
                    id: statusItem;

                    width: parent.width;
                    height: Math.max(serverNameText.height, indicatorsRow.height);

                    Base.Hyperlink
                    {
                        id: serverNameText;

                        clip: true;
                        anchors
                        {
                            left: parent.left;
                            right: indicatorsRow.left;
                            rightMargin: Common.SizeManager.spacing.base;

                            verticalCenter: parent.verticalCenter;
                        }

                        wrapMode: Text.Wrap;
                        caption: serverName;

                        readonly property bool linkAvailable: (addr.length
                            && rtuContext.isDiscoverableFromCurrentNetwork(addr, "")
                            && !operationText.visible && thisComponent.availableByHttp);

                        readonly property string addr: thisComponent.displayAddress;
                        hyperlink: (linkAvailable ? ("http://%1:%2").arg(displayAddress).arg(port) : "");

                        font.pixelSize: Common.SizeManager.fontSizes.base;
                    }

                    Row
                    {
                        id: indicatorsRow;

                        anchors
                        {
                            right: parent.right;
                            verticalCenter: parent.verticalCenter;
                        }

                        Image
                        {
                            id: safeModeIndicator;

                            visible: thisComponent.safeMode;

                            width: height;
                            height: Common.SizeManager.clickableSizes.base * 0.7;
                            anchors.verticalCenter: parent.verticalCenter;

                            source: "qrc:/resources/safe.png";
                        }

                        Image
                        {
                            id: noHddIndicator;

                            visible: !hasHdd;

                            width: height;
                            height: Common.SizeManager.clickableSizes.base * 0.7;

                            anchors.verticalCenter: parent.verticalCenter;
                            source: "qrc:/resources/no-hdd.png";
                        }
                    }
                }

                Item
                {
                    opacity: (thisComponent.availableForSelection ? 1 : 0.5);

                    width: parent.width;
                    height: Math.max(versionText.height, osText.height);

                    Base.Text
                    {
                        id: versionText;

                        clip: true;
                        anchors
                        {
                            left: parent.left;
                            verticalCenter: parent.verticalCenter;
                        }

                        text: qsTr("v%1").arg(thisComponent.version);
                        font.pixelSize: Common.SizeManager.fontSizes.small;
                    }

                    Base.Text
                    {
                        id: osText;

                        clip: true;
                        anchors
                        {
                            left: versionText.right;
                            right: parent.right;
                            leftMargin: Common.SizeManager.spacing.base;
                            verticalCenter: parent.verticalCenter;
                        }

                        horizontalAlignment: Text.AlignRight;
                        wrapMode: Text.Wrap;
                        text: thisComponent.os;
                        font.pixelSize: Common.SizeManager.fontSizes.small;
                    }
                }

                Item
                {
                    opacity: (thisComponent.availableForSelection ? 1 : 0.5);

                    width: parent.width;
                    height: Math.max(addressText.height, hardwareAddressText.height);

                    Base.Text
                    {
                        id: addressText;

                        clip: true;
                        anchors
                        {
                            left: parent.left;
                            verticalCenter: parent.verticalCenter;
                        }

                        text: (thisComponent.displayAddress.length ?
                            thisComponent.displayAddress : qsTr("Identifying IP.."));
                        font.pixelSize: Common.SizeManager.fontSizes.small;
                    }

                    Base.Text
                    {
                        id: hardwareAddressText;

                        clip: true;
                        anchors
                        {
                            left: addressText.right;
                            right: parent.right;
                            leftMargin: Common.SizeManager.spacing.base;
                            verticalCenter: parent.verticalCenter;
                        }

                        horizontalAlignment: Text.AlignRight;
                        wrapMode: Text.Wrap;
                        text: (thisComponent.hardwareAddress.length
                            ? thisComponent.hardwareAddress : qsTr("Identifying mac address"));
                        font.pixelSize: Common.SizeManager.fontSizes.small;
                    }
                }

                Text
                {
                    id :operationText;

                    visible: thisComponent.operation.length;
                    width: parent.width;

                    wrapMode: Text.Wrap;
                    text: thisComponent.operation;
                }
            }
        }

        Base.LineDelimiter
        {
            color: "#e4e4e4";
        }
    }

    Rtu.Mark
    {
        selected: (selectedState === Qt.Checked);
        anchors.fill: parent;
    }
}

