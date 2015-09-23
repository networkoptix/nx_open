import QtQuick 2.1;
import QtQuick.Controls 1.1;

import "../controls/base" as Base;
import "../common" as Common;

import networkoptix.rtu 1.0 as NxRtu;

Item
{
    id: thisComponent;

    property string caption;
    property int changesCount;
    property int currentCount;

    anchors
    {
        left: (parent ? parent.left : undefined);
        right: (parent ? parent.right : undefined);
        top: (parent ? parent.top : undefined);
        bottom: (parent ? parent.bottom: undefined);

        leftMargin: Common.SizeManager.spacing.medium;
        topMargin: Common.SizeManager.spacing.medium;
    }

    Base.Column
    {
        anchors
        {
            left: parent.left;
            right: parent.right;
            top: parent.top;
            bottom: buttonsPanel.bottom;

            rightMargin: Common.SizeManager.spacing.medium;
            bottomMargin: Common.SizeManager.spacing.medium;
        }

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

        Base.Text
        {
            id: patienceText;

            visible: false;

            font.pixelSize: Common.SizeManager.fontSizes.medium;
            text: qsTr("Please be patient, the operation can take up to several minutes");
        }

        Timer
        {
            id: patienceTimer;

            interval: 5 * 1000;
            repeat: false;

            onTriggered:
            {
                patienceText.visible = true;
                patienceTimer.stop();
            }

            Component.onCompleted: { patienceTimer.start(); }
        }
    }

    Item
    {
        id: buttonsPanel;

        height: Common.SizeManager.clickableSizes.medium * 2;
        anchors
        {
            left: parent.left;
            right: parent.right;
            bottom: parent.bottom;
        }

        Base.ThickDelimiter
        {
            id: delimiter;

            anchors
            {
                left: parent.left;
                right: parent.right;
                top: parent.top;
            }
        }

        Base.StyledButton
        {
            id: minimizeButton;

            height: Common.SizeManager.clickableSizes.medium;
            width: height * 6;
            anchors
            {
                right: parent.right;
                verticalCenter: parent.verticalCenter;

                rightMargin: Common.SizeManager.spacing.medium;
            }

            text: qsTr("Change other settings");

            onClicked:
            {
                rtuContext.hideProgressTask();
            }
        }
    }
}
