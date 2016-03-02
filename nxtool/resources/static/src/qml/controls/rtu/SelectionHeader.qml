import QtQuick 2.3;

import "../base" as Base;
import "../rtu" as Rtu;
import "../../common" as Common;

Base.Column
{
    id: thisComponent;

    signal selectAllServers(bool select);
    
    property alias selectAllCheckedState: selectAllCheckBox.selectAllCheckedState;
    
    anchors
    {
        left: parent.left;
        right: parent.right;

        leftMargin: Common.SizeManager.spacing.medium;
        rightMargin: Common.SizeManager.spacing.medium;
    }

    Rectangle
    {
        color: "#1DA6DC";

        height: icon.height;
        anchors
        {
            left: parent.left;
            right: parent.right;
            leftMargin: -Common.SizeManager.spacing.medium;
            rightMargin: -Common.SizeManager.spacing.medium;
        }


        Image
        {
            id: icon;
            source: "qrc:/resources/section_header_icon.png";

            width: Common.SizeManager.clickableSizes.large * 2;
            height: width;
            anchors
            {
                left: parent.left;
                leftMargin: Common.SizeManager.spacing.base;
                verticalCenter: parent.verticalCenter;
            }
        }

        Column
        {
            anchors
            {
                left: icon.right;
                right: parent.right;
                rightMargin: Common.SizeManager.spacing.base;
                verticalCenter: parent.verticalCenter;
            }

            Base.Text
            {
                anchors
                {
                    left: parent.left;
                    right: parent.right;
                }

                color: "white";
                wrapMode: Text.Wrap;
                font.pixelSize: Common.SizeManager.fontSizes.large;
                horizontalAlignment: Text.AlignLeft;

                text: qsTr("Auto-Detected");
            }

            Base.Text
            {
                anchors
                {
                    left: parent.left;
                    right: parent.right;
                }

                color: "white";
                wrapMode: Text.Wrap;
                font.pixelSize: Common.SizeManager.fontSizes.large;
                horizontalAlignment: Text.AlignLeft;

                text: qsTr("Servers and Systems");
            }
        }
    }

    Rtu.SelectAllCheckbox
    {
        id: selectAllCheckBox;

        onSelectAllServers: { thisComponent.selectAllServers(select); }
    }
    
    Base.LineDelimiter
    {
        color: "#e4e4e4";
    }
}
