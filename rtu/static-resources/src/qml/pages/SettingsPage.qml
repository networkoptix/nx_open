import QtQuick 2.1;
import QtQuick.Controls 1.1;

import "../common" as Common
import "../settings" as Settings;
import "../controls/base" as Base;

Item
{
    id: thisComponent;
    
    property var selectedServersModel;
    property bool parametersChanged: systemAndPasswordSettings.changed 
        || ipPortSettings.changed || dateTimeSettings.changed;

    anchors.fill: parent;
   
    function applyChanges()
    {
        var children = settingsColumn.children;
        var childrenCount = children.length;
        var changesCount = 0;
        var entitiesCount = 0;
        for (var i = 0; i !== childrenCount; ++i)
        {
            var child = children[i];
            if (!child.hasOwnProperty("tryApplyChanges"))
                continue;
            ++entitiesCount;
            
            if (!child.tryApplyChanges())
                break;
            ++changesCount;
        }

        if (changesCount && (changesCount == entitiesCount))
        {
            rtuContext.changesManager().applyChanges();
        }
        else
        {
            rtuContext.changesManager().clearChanges();
        }
    }
    
    ScrollView
    {
        anchors
        {
            left: parent.left;
            right: parent.right;
            top: parent.top;
            bottom: buttonsPanel.top;
        }
        
        Flickable
        {
            clip: true;
    
            anchors.fill: parent;
            flickableDirection: Flickable.VerticalFlick;
            contentHeight: settingsColumn.height + settingsColumn.anchors.topMargin
                + settingsColumn.anchors.bottomMargin;
    
            Column
            {
                id: settingsColumn;
    
                spacing: Common.SizeManager.spacing.large;
                anchors
                {
                    left: parent.left;
                    right: parent.right;
                    top: parent.top;
    
                    topMargin: Common.SizeManager.fontSizes.base;
                    leftMargin: Common.SizeManager.spacing.medium;
                    rightMargin: Common.SizeManager.spacing.medium;
                }
    
                Settings.IpPortSettings
                {
                    id: ipPortSettings;
                }
    
                Settings.DateTimeSettings
                {
                    id: dateTimeSettings;
                }
    
                Settings.SystemAndPasswordSetting
                {
                    id: systemAndPasswordSettings;
                }
                
                Item
                {
                    width: 1;
                    height: Common.SizeManager.spacing.base;
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

        Row
        {
            id: buttonsRow;

            spacing: Common.SizeManager.spacing.base;
            
            anchors
            {
                left: parent.left;
                right: parent.right;
                top: delimiter.bottom;
                bottom: parent.bottom;
                leftMargin: Common.SizeManager.spacing.medium;
                rightMargin: Common.SizeManager.spacing.medium;
            }

            layoutDirection: Qt.RightToLeft;
    
            Base.StyledButton
            {
                id: applyButton;
                text: qsTr("Apply changes");
    
                height: Common.SizeManager.clickableSizes.medium;
                width: height * 4;
                
                anchors.verticalCenter: parent.verticalCenter;
                enabled: systemAndPasswordSettings.changed || ipPortSettings.changed || dateTimeSettings.changed;
                
                onClicked: thisComponent.applyChanges();
            }
    
            Base.Button
            {
                text: "Cancel";
    
                anchors.verticalCenter: parent.verticalCenter;
                height: Common.SizeManager.clickableSizes.medium;
                width: height * 3;
                
                enabled: applyButton.enabled;
                onClicked: rtuContext.selectionChanged();
            }
        }
    }
}

