import QtQuick 2.0

import "../rtu" as Rtu;
import "../base" as Base;

import "../../common" as Common;

Base.Column
{
    id: thisComponent;

    signal selectAllServers(bool select);

    property alias selectAllCheckedState: selectAllCheckBox.selectAllCheckedState;

    width: parent.width;

    Base.EmptyCell {}

    Rtu.SelectAllCheckbox
    {
        id: selectAllCheckBox;

        onSelectAllServers: { thisComponent.selectAllServers(select); }
    }

    Base.EmptyCell {}
}
