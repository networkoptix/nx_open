import QtQuick 2.1

import "../common" as Common;
import "../controls/base" as Base;

import networkoptix.rtu 1.0 as NxRtu;

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

    Base.Column
    {
        id: copyrightHolder;

        spacing: Common.SizeManager.spacing.small;

        anchors
        {
            right: parent.right;
            bottom: parent.bottom;

            rightMargin: Common.SizeManager.spacing.medium;
            bottomMargin: Common.SizeManager.spacing.medium;
        }

        TextInput
        {
            id: softwareVersion;

            property string versionTemplate: (rtuContext.isBeta ?
                qsTr("%1 beta (%2)") : qsTr("%1 (%2)"));
            property string version: versionTemplate.arg(
                rtuContext.toolVersion).arg(rtuContext.toolRevision);

            anchors.right: parent.right;

            readOnly: true;
            selectByMouse: true;
            color: "#666666";
            font.pixelSize: Common.SizeManager.fontSizes.small;
            text: qsTr("     Software version %1").arg(version);

        }

        Base.Hyperlink
        {
            anchors.right: parent.right;

            hyperlink: rtuContext.toolCompanyUrl;
        }

        Base.Hyperlink
        {
            anchors.right: parent.right;

            caption: rtuContext.toolSupportMail;
            hyperlink: qsTr("mailto:%1?Subject=Support request [%2 ver.%3]").arg(
                rtuContext.toolSupportMail).arg(rtuContext.toolDisplayName).arg(softwareVersion.version);
        }
    }
}
