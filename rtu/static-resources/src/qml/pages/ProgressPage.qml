import QtQuick 2.1;
import QtQuick.Controls 1.1;

import "../controls/base" as Base;
import "../common" as Common;

Base.Column
{
    id: thisComponent;
    
    anchors.fill: parent;

    Base.Text
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


    Base.LineDelimiter { }

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
