import QtQuick 2.4;

import "../controls";
import "settings"

import "standard" as Rtu;

Item
{
    id: thisComponent;
    
    property bool parametersChanged: false;
    property var selectedServersModel;
    
    Component
    {
        id: emptyPage;

        Text
        {
            anchors.fill: parent;

            verticalAlignment: Text.AlignVCenter;
            horizontalAlignment: Text.AlignHCenter;
            wrapMode: Text.Wrap;

            font.pointSize: SizeManager.fontSizes.large;
            text: qsTr("Please select servers");

            color: "grey";
        }
    }

    Component
    {
        id: settingsPage;

        Item
        {
            anchors.fill: parent;
        
            Flickable
            {
                clip: true;
                anchors
                {
                    left: parent.left;
                    right: parent.right;
                    top: parent.top;
                    bottom: buttonsPanel.top;
                }
                
                flickableDirection: Flickable.VerticalFlick;
                contentHeight: settingsColumn.height + settingsColumn.anchors.topMargin
                    + settingsColumn.anchors.bottomMargin;
            
                Column
                {
                    id: settingsColumn;
        
                    spacing: SizeManager.spacing.base;
                    anchors
                    {
                        left: parent.left;
                        right: parent.right;
                        top: parent.top;
                        
                        topMargin: SizeManager.fontSizes.base;
                        bottomMargin: SizeManager.fontSizes.base;
                    }

                    IpPortSettings 
                    {
                        id: ipPortSettings;
                    }
                    
                    DateTimeSettings 
                    {
                        id: dateTimeSettings;
                    }

                    SystemAndPasswordSetting 
                    {
                        id: systemAndPasswordSettings;
                    }
                    
  
                    UpdatesSettings
                    {
                     //   selectedServersModel: thisComponent.selectedServersModel;
                    }
                    
                }
            }
            
            Item
            {
                id: buttonsPanel;
                
                height: SizeManager.clickableSizes.large;
                anchors
                {
                    left: parent.left;
                    right: parent.right;
                    bottom: parent.bottom;
                }
                
                Row
                {
                    id: buttonsRow;
                    
                    anchors
                    {
                        left: parent.left;
                        right: parent.right;
                        leftMargin: SizeManager.spacing.base;
                        rightMargin: SizeManager.spacing.base;
                    }

                    spacing: SizeManager.spacing.base;
                    layoutDirection: Qt.RightToLeft;
                    
                    Rtu.Button
                    {
                        id: applyButton;
                        text: "Apply changes";
                        
                        enabled: systemAndPasswordSettings.changed || ipPortSettings.changed || dateTimeSettings;
                        
                        onClicked: 
                        {
                            ipPortSettings.applyButtonPressed();
                            systemAndPasswordSettings.applyButtonPressed();
                            dateTimeSettings.applyButtonPressed();
                            
                            rtuContext.changesManager().applyChanges();
                        }
                    }
                    
                    Rtu.Button
                    {
                        text: "Cancel";

                        enabled: applyButton.enabled;
                    }
                }

            }
        }
    }

    Loader
    {
        id: pageLoader;

        anchors.fill: parent;
        sourceComponent: (rtuContext.selectedServersCount !== 0 ? settingsPage : emptyPage);
    }
}

