
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
            height: buttons.height + Common.SizeManager.spacing.base * 2;
            width: parent.width;
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
                    right: buttons.left;
                    leftMargin: Common.SizeManager.spacing.base;
                    rightMargin: Common.SizeManager.spacing.base;
                    verticalCenter: parent.verticalCenter;
                }

                value: model.appliedChangesCount + 1;
                maximumValue: model.totalChangesCount + 1;
            }

            Row
            {
                id: buttons;

                spacing: Common.SizeManager.spacing.base;

                anchors
                {
                    verticalCenter: parent.verticalCenter;
                    right: parent.right;
                    rightMargin: Common.SizeManager.spacing.base;
                }

                Base.Button
                {
                    text: "close"

                    onClicked:
                    {
                        rtuContext.setCurrentProgressTask(null);
                        var task = thisComponent.model.taskAtIndex(model.index);
                        rtuContext.changesManager().removeChangeProgress(task);
                    }
                }

                Base.StyledButton
                {
                    text: "show";

                    onClicked:
                    {
                        var task = thisComponent.model.taskAtIndex(model.index);
                        rtuContext.setCurrentProgressTask(task);
                    }
                }
            }
        }

    }
}
