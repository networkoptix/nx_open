
import QtQuick 2.1;
import QtQuick.Controls 1.1;

import "." as Rtu;
import "../base" as Base;
import "../../common" as Common;

import networkoptix.rtu 1.0 as NxRtu;

ListView
{
    id: thisComponent;

    delegate: Column
    {
        width: parent.width;


        Item
        {
            id: placeholder;

            height: closeButton.height + Common.SizeManager.spacing.base * 2;
            width: parent.width;

            Base.Button
            {
                id: closeButton;
                text: "x"

                height: Common.SizeManager.clickableSizes.base * 2 / 3;
                width: height;

                anchors
                {
                    left: parent.left;
                    leftMargin: Common.SizeManager.spacing.base;
                    verticalCenter: parent.verticalCenter;
                }

                onClicked:
                {
                    var task = thisComponent.model.taskAtIndex(model.index);
                    rtuContext.removeChangeProgress(task);
                }
            }

            Component
            {
                id: progressStatus;

                Item
                {
                    width: parent.width;
                    height: parent.height;

                    Base.Text
                    {
                        id: countText;

                        anchors
                        {
                            left: parent.left;
                            leftMargin: Common.SizeManager.spacing.base;
                            verticalCenter: parent.verticalCenter;
                        }

                        text: qsTr("%1 / %2").arg(model.appliedChangesCount).arg(model.totalChangesCount);
                    }

                    ProgressBar
                    {
                        anchors
                        {
                            left: countText.right;
                            right: parent.right;
                            leftMargin: Common.SizeManager.spacing.base;
                            verticalCenter: parent.verticalCenter;
                        }

                        value: model.appliedChangesCount + 1;
                        maximumValue: model.totalChangesCount + 1;
                    }
                }
            }

            Component
            {
                id: summaryStatus;

                Item
                {
                    width: parent.width;
                    height: parent.height;

                    Base.Text
                    {
                        id: status;

                        anchors
                        {
                            left: parent.left;
                            right: detailsButton.left;
                            leftMargin: Common.SizeManager.spacing.base;
                            rightMargin: Common.SizeManager.spacing.base;
                            verticalCenter: parent.verticalCenter;
                        }

//                        qsTr("%1 of %2 successful changed").arg(model.changesCount).arg(
//                            failedSummary.model.changesCount + model.changesCount);
                        text: "Some state";
                    }

                    Base.StyledButton
                    {
                        id: detailsButton;

                        text: "details";

                        anchors
                        {
                            right: parent.right;
                            verticalCenter: parent.verticalCenter;
                        }

                        onClicked:
                        {
                            var task = thisComponent.model.taskAtIndex(model.index);
                            rtuContext.showDetailsForTask(task);
                        }
                    }
                }
            }

            Loader
            {
                id: meatLoader;

                height: placeholder.height;
                anchors
                {
                    left: closeButton.right;
                    right: parent.right;
                    leftMargin: Common.SizeManager.spacing.base;
                    rightMargin: Common.SizeManager.spacing.base;
                    verticalCenter: parent.verticalCenter;
                }

                sourceComponent: (model.inProgress ? progressStatus : summaryStatus);
            }
        }
    }
}
