import QtQuick 2.4;
import QtQuick.Window 2.2;
import QtQuick.Controls 1.3;

import "controls";

import "controls/standard" as Rtu;
import "pages" as Pages;

Window
{
    id: mainWindow;
    
    width: 1024;
    height: 600;
    visible: true;
    
    Component
    {
        id: settingsPageComponent;
        
        SettingsPage
        {
            selectedServersModel: serversSelectionListView.selectedServersModel;
            id: settingsPage;
        }
    }
    
    Component
    {
        id: progressPageComponent;
        
        Pages.ProgressPage {}
    }
    
    Component
    {
        id: summaryPageComponent;
        
        Pages.SummaryPage {}
    }

    SplitView
    {
        anchors.fill: parent;
        orientation: Qt.Horizontal;
        
        
        ServersSelectionListView
        {
            id: serversSelectionListView;
            askForSelectionChanges: (loader.item && loader.item.hasOwnProperty("parametersChanged") 
                ? loader.item.parametersChanged : false);
            
            enabled: (rtuContext.currentPage === 0);
            opacity: enabled ? 1 : 0.5;
        }
        
        Loader
        {
            id: loader;
            
            sourceComponent: settingsPageComponent;
            Connections
            {
                target: rtuContext;
                onCurrentPageChanged: { loader.reloadPage(); }
                onSelectionChagned: { loader.reloadPage(); }
            }
            
            function reloadPage()
            {
                loader.sourceComponent = undefined;
                switch(rtuContext.currentPage)
                {
                case 1:
                    loader.sourceComponent = progressPageComponent;
                    break;
                case 2:
                    loader.sourceComponent = summaryPageComponent;
                    break;
                default:
                    loader.sourceComponent = settingsPageComponent;
                }
            }
        }
    }
    
    Component.onCompleted: 
    {
        serversSelectionListView.width = mainWindow.width / 3;
    }
}
