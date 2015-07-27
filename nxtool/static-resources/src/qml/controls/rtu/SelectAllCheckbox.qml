import QtQuick 2.0

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
        spacing: Common.SizeManager.spacing.base;

        anchors.left: parent.left;

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
