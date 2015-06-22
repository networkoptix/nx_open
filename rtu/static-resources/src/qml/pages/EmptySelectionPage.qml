import QtQuick 2.1

import "../common" as Common;
import "../controls/base" as Base;

Item
{
    id: thisComponent;

    property real minWidth: holder.width + Common.SizeManager.spacing.base;
    property real minHeight: holder.height + Common.SizeManager.spacing.base;
    
    anchors.fill: parent;
    
    Base.Column
    {
        id: holder;
        
        anchors.centerIn: parent;
        
        Base.Text
        {
            anchors.horizontalCenter: parent.horizontalCenter;
            
            color: "#666666";
            text: qsTr("Welcome to the Nx1 Setup Tool.");
            font.pixelSize: Common.SizeManager.fontSizes.medium;
            wrapMode: Text.Wrap;
    
        }
        
        Image 
        {
            width: Common.SizeManager.clickableSizes.large * 3;
            height: width * sourceSize.height / sourceSize.width;
            
            anchors.horizontalCenter: parent.horizontalCenter;
            
            source: "qrc:/resources/empty_page.png";
        }
        
        Base.Text
        {
            anchors.horizontalCenter: parent.horizontalCenter;
    
            color: "#666666";
            text: qsTr("Select Nx1 device(s) from the\nmenu on the left to get started");
            font.pixelSize: Common.SizeManager.fontSizes.medium;
        }
    }
}
