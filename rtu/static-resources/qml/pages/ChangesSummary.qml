import QtQuick 2.4;

import "../controls" as Common;
import "../controls/standard" as Rtu;

Common.ExpandableWithHeader
{
    id: thisComponent;

    property int changesCount: 0;
    property string captionTemplate;
    property var model;

    headerText: captionTemplate.arg(changesCount);
    visible: changesCount;

    areaDelegate: Column
    {
        spacing: Common.SizeManager.spacing.large;

        anchors
        {
            left: parent.left;
            right: parent.right;
            leftMargin: Common.SizeManager.spacing.base;
        }

        Item
        {
            id: spacingItem;
            height: 1;
            width: 1;
        }

        Repeater
        {
            id: repeater;

            model: thisComponent.model;

            delegate: Column
            {
                spacing: Common.SizeManager.spacing.medium;

                anchors
                {
                    left: parent.left;
                    right: parent.right;
                }

                Rtu.Text
                {
                    text: model.serverName;
                    font.bold: true;
                }

                RequestResultDelegate
                {
                    model: resultsModel;
                }
            }
        }
    }

}
