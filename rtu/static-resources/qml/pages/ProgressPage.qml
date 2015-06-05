import QtQuick 2.4;
import QtQuick.Controls 1.3;

import "../controls/standard" as Rtu;
import "../controls" as Common;

Column
{
    id: thisComponent;
    
    anchors.fill: parent;

    spacing: Common.SizeManager.spacing.base;

    Rtu.Text
    {
        id: caption;

        anchors
        {
            left: parent.left;
            right: parent.right;
            leftMargin: Common.SizeManager.spacing.base;
            rightMargin: Common.SizeManager.spacing.base;
        }

        text: qsTr("Applying changes...");
        font
        {
            bold: true;
            pointSize: Common.SizeManager.fontSizes.large;
        }
    }


    Common.LineDelimiter { }

    ProgressBar
    {
        id: changesProgressBar;

        height: Common.SizeManager.clickableSizes.base;
        anchors
        {
            left: parent.left;
            right: parent.right;
            leftMargin: Common.SizeManager.spacing.base;
            rightMargin: Common.SizeManager.spacing.base;
        }

        minimumValue: 0;
        maximumValue: rtuContext.changesManager().totalChangesCount;
        value: rtuContext.changesManager().appliedChangesCount;
    }

}
