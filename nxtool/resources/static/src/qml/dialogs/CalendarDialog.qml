import QtQuick 2.0;
import QtQuick.Controls 1.3;

import "." as Dialogs;
import "../common" as Common;
import "../controls/rtu" as Rtu;

import networkoptix.rtu 1.0 as NxRtu;

Dialogs.Dialog
{
    id: thisComponent;

    signal dateChanged(date newDate);

    property date initDate;

    onButtonClicked:
    {
        if (id & NxRtu.Buttons.Ok)
            dateChanged(thisComponent.area.date);
    }

    onVisibleChanged:
    {
        if (visible)
            thisComponent.area.date = initDate;
    }

    title: qsTr("Select date...");

    buttons: NxRtu.Buttons.Ok | NxRtu.Buttons.Cancel;
    styledButtons: NxRtu.Buttons.Ok;
    cancelButton: NxRtu.Buttons.Cancel;

    areaDelegate: Item
    {
        property alias date: calendar.selectedDate;

        width: calendar.width + Common.SizeManager.spacing.base * 2;
        height: calendar.height + Common.SizeManager.spacing.base * 2;

        Calendar
        {
            id: calendar;
            anchors.centerIn: parent;
        }
    }
}
