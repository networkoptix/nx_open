import QtQuick 2.1;
import QtQuick.Window 2.0;
import QtQuick.Controls 1.1;

import networkoptix.rtu 1.0 as NxRtu;

import "pages" as Pages;
import "common" as Common;

Window
{
    id: mainWindow;
    
    width: 1024;
    height: 600;
    visible: true;
    
    Component
    {
        id: emptyPage;

        Pages.EmptySelectionPage {}
    }

    Component
    {
        id: settingsPageComponent;
        
        Pages.SettingsPage
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
        
        
        Pages.ServerSelectionPage
        {
            id: serversSelectionListView;
            askForSelectionChanges: (loader.item && loader.item.hasOwnProperty("parametersChanged")
                ? loader.item.parametersChanged : false);
            

            enabled: (rtuContext.currentPage === NxRtu.Constants.SettingsPage);
            opacity: enabled ? 1 : 0.5;
        }
        
        Loader
        {
            id: loader;
            
            sourceComponent: emptyPage;
            Connections
            {
                target: rtuContext;
                onCurrentPageChanged: { loader.reloadPage(); }
                onSelectionChanged: { loader.reloadPage(); }
            }
            
            function reloadPage()
            {
                loader.sourceComponent = undefined;
                switch(rtuContext.currentPage)
                {
                case NxRtu.Constants.ProgressPage:
                    loader.sourceComponent = progressPageComponent;
                    return;
                case NxRtu.Constants.SummaryPage:
                    loader.sourceComponent = summaryPageComponent;
                    return;
                }

                loader.sourceComponent = (rtuContext.selection && (rtuContext.selection !== null)
                    && rtuContext.selection.count ? settingsPageComponent : emptyPage);
            }
        }
    }
    
    Component.onCompleted: 
    {
        serversSelectionListView.width = mainWindow.width / 3;
    }
}
