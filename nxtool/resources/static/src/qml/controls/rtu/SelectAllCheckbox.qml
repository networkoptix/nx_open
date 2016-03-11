import QtQuick 2.3

import "../base" as Base;
import "../../common" as Common;

Item
{
    id: thisComponent;

    property int selectAllCheckedState: 0;

    signal selectAllServers(bool select);

    height: row.height + Common.SizeManager.spacing.base * 2;

    width: parent.width;

    Row
    {
        id: row;
        spacing: Common.SizeManager.spacing.medium;

        anchors
        {
            left: parent.left;
            verticalCenter: parent.verticalCenter;
        }


        Base.CheckBox
        {
            id: selectAllCheckbox;

            activeFocusOnPress: false;
            activeFocusOnTab: false;

            anchors.verticalCenter: parent.verticalCenter;
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
            anchors.verticalCenter: parent.verticalCenter;
            text: qsTr("Select / Deselect All");
            font.pixelSize: Common.SizeManager.fontSizes.medium;
            color: "#666666";
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
