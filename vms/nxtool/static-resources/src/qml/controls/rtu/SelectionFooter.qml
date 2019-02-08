import QtQuick 2.0

import "../rtu" as Rtu;
import "../base" as Base;

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

    Rtu.SelectAllCheckbox
    {
        id: selectAllCheckBox;

        onSelectAllServers: { thisComponent.selectAllServers(select); }
    }

    Base.EmptyCell {}
}
