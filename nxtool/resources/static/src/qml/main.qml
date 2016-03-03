import QtQuick 2.3;
import QtQuick.Window 2.0;
import QtQuick.Controls 1.1;
import QtQuick.Layouts 1.0;

import networkoptix.rtu 1.0 as NxRtu;

import "pages" as Pages;
import "controls/rtu" as Rtu;

Window
{
    id: mainWindow;
    
    title: rtuContext.toolDisplayName + (rtuContext.isBeta ? " (beta)" : "");
        
    width: 1024;
    height: 600;
    
    color: "white";

    minimumHeight: 480;
    minimumWidth: 1024;
    
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
            id: settingsPage;
            selectedServersModel: selectionPage.selectedServersModel;
            
            Connections
            {
                target: selectionPage;
                onApplyChanges: { settingsPage.applyChanges(); }
            }
        }
    }
    
    Component
    {
        id: progressPageComponent;
        
        Pages.ProgressPage 
        {
            property var currentTask: rtuContext.progressTask;

            caption: currentTask.progressPageHeaderText.arg(currentCount.toString()).arg(changesCount.toString());
            changesCount: (currentTask ? currentTask.totalChangesCount : 0);
            currentCount: (currentTask ? currentTask.appliedChangesCount : 0);
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
        
        SplitView
        {
            id: selectionSplit;
            orientation: Qt.Vertical;

            Pages.ServerSelectionPage
            {
                id: selectionPage;
                askForSelectionChange: (loader.item && loader.item.hasOwnProperty("parametersChanged")
                    ? loader.item.parametersChanged : false);

                opacity: enabled ? 1 : 0.5;

                activeFocusOnTab: false;
                Layout.fillHeight: true;
            }

            Rtu.ProgressListView
            {
                id: progressListView;

                outsideSizeChange: selectionSplit.resizing;

                model: rtuContext.changesManager().changesProgressModelObject();

                onShowDetails:
                {
                    selectionPage.tryChangeSelection(function()
                    {
                        rtuContext.showProgressTaskFromList(index);
                    });
                }
            }
        }

        Item
        {
            id: holder;

            Loader
            {
                id: loader;
                
                anchors.fill: parent;
                Layout.minimumWidth: (item && item.minWidth ? item.minWidth : 0);
                Layout.minimumHeight: (item && item.minHeight ? item.minHeight : 0);
                
                sourceComponent: emptyPage;
                    
                Connections
                {
                    target: rtuContext;
                    onCurrentPageChanged: { loader.reloadPage(); }
                    onSelectionChanged:
                    {
                        if (rtuContext.currentPage !== NxRtu.Constants.ProgressPage)
                            loader.reloadPage();
                    }
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

    }
    
    Component.onCompleted: 
    {
        selectionSplit.width = mainWindow.width / 3.4;
        selectionSplit.Layout.minimumWidth = selectionPage.width;
    }
}
