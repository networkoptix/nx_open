import QtQuick 2.1;
import QtQuick.Controls 1.1;

import "../common" as Common;
import "../controls/base" as Base;
import "../controls/rtu" as Rtu;

import networkoptix.rtu 1.0 as NxRtu;

Item
{
    id: thisComponent;
    
    anchors.fill: parent;

    ScrollView
    {
        anchors
        {
            left: parent.left;
            right: parent.right;
            top: parent.top;
            bottom: buttonsPanel.top;
        }
        
        activeFocusOnTab: false;
        Flickable
        {
            id: flickable;
            clip: true;
    
            anchors
            {
                fill: parent;
                leftMargin: Common.SizeManager.spacing.medium;
                rightMargin: Common.SizeManager.spacing.medium;
                topMargin: Common.SizeManager.spacing.medium;
                bottomMargin: Common.SizeManager.spacing.medium;
            }                
            
            flickableDirection: Flickable.VerticalFlick;
            contentWidth: content.width;
            contentHeight: content.height;
        
            Base.Column
            {
                id: content;
                
                spacing: Common.SizeManager.spacing.large;

                width: flickable.width;
                
                Rtu.ChangesSummary
                {
                    id: summary;
                    
                    visible: model.changesCount;
                    caption: (model.changesCount == 1 ? qsTr("1 operation is successfully completed")
                        : qsTr("%1 of %2 operations are successfully completed")
                            .arg(model.changesCount).arg(failedSummary.model.changesCount + model.changesCount));
                    anchors
                    {
                        left: parent.left;
                        right: parent.right;
                    }

                    model: rtuContext.progressTask.successfulModel;
                    expanded: !failedSummary.model.changesCount;
                }
                
                Rtu.ChangesSummary
                {
                    id: failedSummary;

                    anchors
                    {
                        left: parent.left;
                        right: parent.right;
                    }
                    successfulSummary: false;
                    visible: model.changesCount;
                    caption: qsTr("%1 errors").arg(model.changesCount);
                    model: rtuContext.progressTask.failedModel;
                }
            }
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
            id: okButton;
            
            height: Common.SizeManager.clickableSizes.medium;
            width: height * 3;
            anchors
            {
                right: parent.right;
                verticalCenter: parent.verticalCenter;
                
                rightMargin: Common.SizeManager.spacing.medium;
            }
    
            text: qsTr("Ok");
            onClicked: { rtuContext.hideProgressTask(); }
            Component.onCompleted: { forceActiveFocus(); }
        }
    }
    
}
