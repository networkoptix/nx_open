import QtQuick 2.0;

import "../base" as Base;
import "../../common" as Common;

Base.Column
{
    id: thisComponent;

    signal selectAllServers(bool select);
    
    property int selectAllCheckedState: 0;
    
    anchors
    {
        left: parent.left;
        right: parent.right;

        leftMargin: Common.SizeManager.spacing.medium;
        rightMargin: Common.SizeManager.spacing.medium;
    }
    
    Base.EmptyCell {}

    Base.Text
    {
        id: headerText;

        anchors
        {
            left: parent.left;
            right: parent.right;
        }

        color: "#666666";
        wrapMode: Text.Wrap;
        font.pixelSize: Common.SizeManager.fontSizes.large;
        horizontalAlignment: Text.AlignHCenter;

        text: qsTr("Auto-Detected Nx1 Devices and Systems.");
    }
    
    Item
    {
        height: row.height + Common.SizeManager.spacing.base * 2;
        
        anchors
        {
            left: parent.left;
            right: parent.right;
        }

        Row
        {
            id: row;
            spacing: Common.SizeManager.spacing.base;
            
            anchors.centerIn: parent;
            
            Base.CheckBox
            {
                id: selectAllCheckbox;
                
        
                Binding
                {
                    target: selectAllCheckbox;
                    property: "checkedState";
                    value: thisComponent.selectAllCheckedState;
                }

                onClicked: 
                {
                    thisComponent.selectAllServers(checkedState === Qt.Unchecked); 
                }
            }
            
            Base.Text
            {
                text: qsTr("select / unselect all");
            }
        }
        
        MouseArea
        {
            anchors.fill: parent;

            onClicked: 
            {
                thisComponent.selectAllServers(selectAllCheckbox.checkedState === Qt.Unchecked); 
            }
        }
    }

    
    Base.LineDelimiter
    {
        color: "#e4e4e4";
    }
}
