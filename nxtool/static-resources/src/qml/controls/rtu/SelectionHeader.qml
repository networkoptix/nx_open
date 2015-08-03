import QtQuick 2.0;

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

        text: qsTr("Auto-Detected Servers and Systems.");
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
