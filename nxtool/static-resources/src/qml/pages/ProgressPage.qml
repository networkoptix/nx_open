import QtQuick 2.1;
import QtQuick.Controls 1.1;

import "../controls/base" as Base;
import "../common" as Common;

Base.Column
{
    id: thisComponent;

    anchors
    {
        left: (parent ? parent.left : undefined);
        right: (parent ? parent.right : undefined);
        top: (parent ? parent.top : undefined);
        bottom: (parent ? parent.bottom : undefined);
        
        leftMargin: Common.SizeManager.spacing.medium;
        rightMargin: Common.SizeManager.spacing.medium;
        topMargin: Common.SizeManager.spacing.medium;
        bottomMargin: Common.SizeManager.spacing.medium;
    }           

    property string caption;
    property int changesCount;
    property int currentCount;

    Base.Text
    {
        id: caption;

        anchors
        {
            left: parent.left;
            right: parent.right;
        }

        text: thisComponent.caption;
        thin: false;
        wrapMode: Text.Wrap;
        font.pixelSize: Common.SizeManager.fontSizes.large;
    }


    Base.LineDelimiter 
    {
        color: "#e4e4e4";
    }

    Rectangle
    {
        height: Common.SizeManager.clickableSizes.base;

        anchors
        {
            left: parent.left;
            right: parent.right;
        }

        border.color: "grey";

        ProgressBar
        {
            id: changesProgressBar;

            indeterminate: true;
            height: parent.height;

            minimumValue: 0;
            maximumValue: 1;
            value: currentCount;

            width: parent.width * (currentCount + 1) / (changesCount + 1);
        }

    }
}
