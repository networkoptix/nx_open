
import QtQuick 2.1;
import QtQuick.Controls 1.1;
import QtQuick.Layouts 1.0;

import "." as Rtu;
import "../base" as Base;
import "../../common" as Common;

import networkoptix.rtu 1.0 as NxRtu;

Item
{
    id: thisComponent;

    visible: (progressListView.count != 0);

    property var model;
    property bool outsideSizeChange: false;

    signal showDetails(int index);

    property real userHeight: 0;
    Layout.minimumHeight: header.height;

    height: Layout.minimumHeight;

    onHeightChanged:
    {
        if (outsideSizeChange)
            userHeight = height;
    }

    function setMaximized(maximized)
    {
        if (maximized)
        {
            var newHeight = progressListView.contentHeight
                + header.height + Common.SizeManager.spacing.base * 2;

            if (!thisComponent.userHeight)
                thisComponent.height = newHeight;
            else if (thisComponent.height < newHeight)
                thisComponent.height = newHeight;
            else
                thisComponent.height = thisComponent.userHeight;
        }
        else
            thisComponent.height = Layout.minimumHeight;
    }

    Column
    {
        id: header;

        width: parent.width;
        spacing: Common.SizeManager.spacing.base;

        Base.ThickDelimiter
        {
            id: topDelimiter;

            anchors
            {
                left: parent.left;
                right: parent.right;
            }
        }

        Item
        {
            width: parent.width;
            height: Math.max(changesSummary.height, stateButton.height)
                + Common.SizeManager.spacing.base;

            Base.Text
            {
                id: changesSummary;

                property int completedCount: progressListView.model.completedCount;
                property int totalCount: progressListView.count;

                anchors
                {
                    left: parent.left;
                    right: stateButton.right;
                    leftMargin: Common.SizeManager.spacing.base;
                    rightMargin: Common.SizeManager.spacing.base;
                    verticalCenter: parent.verticalCenter;
                }

                clip: true;
                thin: false;
                text: qsTr("Changesets: %1 of %2 completed").arg(completedCount).arg(totalCount);
            }

            Base.Button
            {
                id: stateButton;

                property bool maximized: ((progressListView.contentHeight <
                    (thisComponent.height - header.height - Common.SizeManager.spacing.base)));

                visible: ((changesSummary.totalCount != 0) || maximized);
                width: height;

                anchors
                {
                    right: parent.right;
                    rightMargin: Common.SizeManager.spacing.base;
                    verticalCenter: parent.verticalCenter;
                }

                text: "v"
                rotation: (maximized ? 0 : 180);

                onClicked: { thisComponent.setMaximized(!maximized); }
            }
        }

        Base.LineDelimiter
        {
            id: delimiter;

            color: "#666666";

            anchors
            {
                leftMargin: Common.SizeManager.spacing.base;
                rightMargin: Common.SizeManager.spacing.base;
            }
        }
    }

    ScrollView
    {
        id: scrollView;
        clip: true;

        anchors
        {
            left: parent.left;
            right: parent.right;
        }

        y: (header.height + Common.SizeManager.spacing.base)
        height: parent.height - y;

        ListView
        {
            id: progressListView;

            clip: true;
            model: thisComponent.model;

            width: parent.width;

            property int lastCount: 0;
            onCountChanged:
            {
                if (lastCount == count)
                    return;

                lastCount = count;
                if (!lastCount)
                    thisComponent.setMaximized(false);
                else  if (!thisComponent.userHeight || (thisComponent.height < thisComponent.userHeight))
                    thisComponent.setMaximized(true);
            }

            delegate: Column
            {
                id: column;
                spacing: Common.SizeManager.spacing.base;

                anchors
                {
                    left: parent.left;
                    right: parent.right;
                    leftMargin: Common.SizeManager.spacing.base;
                    rightMargin: Common.SizeManager.spacing.base;
                }

                Loader
                {
                    width: parent.width;
                    height: (item ? item.height : 0);

                    sourceComponent: (model.inProgress ? progressStatus : summaryStatus);

                    Component
                    {
                        id: progressStatus;

                        Item
                        {
                            width: (parent ? parent.width : 0);
                            height: Common.SizeManager.clickableSizes.base + Common.SizeManager.spacing.base;

                            Base.Text
                            {
                                id: applyingChangesText;
                                anchors
                                {
                                    left: parent.left;
                                    verticalCenter: parent.verticalCenter;
                                }

                                horizontalAlignment: Text.AlignHCenter;
                                text: qsTr("Applying changes...");
                            }

                            ProgressBar
                            {
                                id: progressBar;

                                anchors
                                {
                                    left: applyingChangesText.right;
                                    right: parent.right;
                                    leftMargin: Common.SizeManager.spacing.base;
                                    verticalCenter: parent.verticalCenter;
                                }

                                indeterminate: true;
                                // TODO: #ynikitenkov decide what is better - just running progress bar or showing stupid progress
                                // value: model.appliedChangesCount + 1;
                                // maximumValue: model.totalChangesCount + 1;
                            }
                        }
                    }

                    Component
                    {
                        id: summaryStatus;

                        Item
                        {
                            width: parent.width;
                            height: Common.SizeManager.clickableSizes.base + Common.SizeManager.spacing.base ;

                            Base.Text
                            {
                                id: status;

                                anchors
                                {
                                    left: parent.left;
                                    right: buttonsPanel.left;
                                    rightMargin: Common.SizeManager.spacing.base;
                                    verticalCenter: parent.verticalCenter;
                                }

                                text: (model.errorsCount ? qsTr("%1 error(s)").arg(model.errorsCount)
                                    : qsTr("Sucessful!"));
                                color: (model.errorsCount ? "red" : "green");
                            }

                            Row
                            {
                                id: buttonsPanel;

                                spacing: Common.SizeManager.spacing.base;
                                anchors
                                {
                                    right: parent.right;
                                    verticalCenter: parent.verticalCenter;
                                }

                                Base.Button
                                {
                                    id: closeButton;

                                    text: qsTr("hide");
                                    onClicked: {  rtuContext.removeProgressTask(model.index); }
                                }

                                Base.StyledButton
                                {
                                    id: detailsButton;

                                    text: qsTr("changelog");
                                    onClicked: { thisComponent.showDetails(model.index); }
                                }
                            }

                        }
                    }
                }

                Base.LineDelimiter
                {
                    color: "#e4e4e4";
                }
            }
        }
    }
}
