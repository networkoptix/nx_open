import QtQuick 2.1;
import QtQuick.Window 2.0;
import QtQuick.Controls 1.1;
import QtQuick.Layouts 1.0;

import networkoptix.rtu 1.0 as NxRtu;

import "pages" as Pages;

Window
{
    id: mainWindow;
    
    title: qsTr("Nx1 Setup & Configuration Tool");
        
    width: 1024;
    height: 600;
    
    color: "white";

    minimumHeight: 480;
    minimumWidth: selectionPage.width 
        + (loader.item && loader.item.minWidth ? loader.item.minWidth : 0);
    
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
            selectedServersModel: selectionPage.selectedServersModel;
        }
    }
    
    Component
    {
        id: progressPageComponent;
        
        Pages.ProgressPage 
        {
            caption: qsTr("Applying changes...");
            changesCount: rtuContext.changesManager().totalChangesCount;
            currentCount: rtuContext.changesManager().appliedChangesCount;
        }
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
            id: selectionPage;
            
            askForSelectionChange: (loader.item && loader.item.hasOwnProperty("parametersChanged")
                ? loader.item.parametersChanged : false);
            
            enabled: (rtuContext.currentPage === NxRtu.Constants.SettingsPage);
            opacity: enabled ? 1 : 0.5;
        }
        
        Loader
        {
            id: loader;
            
            Layout.minimumWidth: (item && item.minWidth ? item.minWidth : 0);
            Layout.minimumHeight: (item && item.minHeight ? item.minHeight : 0);
            
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
                    break;
                case NxRtu.Constants.SummaryPage:
                    loader.sourceComponent = summaryPageComponent;
                    break;
                default:
                    loader.sourceComponent = (rtuContext.selection && (rtuContext.selection !== null)
                        && rtuContext.selection.count ? settingsPageComponent : emptyPage);
                }
            }
        }
    }
    
    Component.onCompleted: 
    {
        selectionPage.width = mainWindow.width / 3.5;
        
        selectionPage.Layout.minimumWidth = selectionPage.width;
    }
}
