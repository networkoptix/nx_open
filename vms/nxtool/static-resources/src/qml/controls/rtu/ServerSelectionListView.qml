import QtQuick 2.1;
import QtQuick.Controls 1.1;
import QtQuick.Layouts 1.0;

import "." as Rtu;
import "../base" as Base;
import "../../common" as Common;
import "../../dialogs" as Dialogs;

import networkoptix.rtu 1.0 as NxRtu;

ListView
{
    id: thisComponent;

    signal applyChanges();
    signal tryChangeSelectedServers(var func);

    property bool askForSelectionChange: false;

    cacheBuffer: 65535;
    clip: true;
    spacing: 1

    model: rtuContext.selectionModel();

    Connections
    {
        target: model;
        onLayoutChanged:
        {
            forceLayout();
        }
    }

    header: Rtu.SelectionHeader 
    {
        selectAllCheckedState: impl.selectAllCheckedState;
        
        onSelectAllServers: { impl.selectAllServers(select); }
    }

    footer: Rtu.SelectionFooter 
    {
        visible: thisComponent.contentHeight > thisComponent.height;

        selectAllCheckedState: impl.selectAllCheckedState;

        onSelectAllServers: { impl.selectAllServers(select); }
    }

    delegate: Loader
    {
        id: meatLoader;

        Component
        {
            id: systemDelegate;
            
            Rtu.SystemItemDelegate 
            {
                itemIndex: index;
                
                systemName: model.systemName;
                loggedState: model.loggedState;
                selectionEnabled: model.enabled;
                selectedState: model.selectedState;

                onSelectionStateShouldBeChanged:
                {
                    thisComponent.tryChangeSelectedServers( function()
                    {
                        thisComponent.model.changeItemSelectedState(index);
                    });
                }

                Connections
                {
                    target: thisComponent.model;
                    onBlinkAtSystem:
                    {
                        if (systemItemIndex == itemIndex)
                            blink();
                    }
                }
            }
        }
    
        Component
        {
            id: serverDelegate;
            
            Rtu.ServerItemDelegate
            {
                port: model.port;
                selectedState: model.selectedState;

                loggedIn: model.loggedIn;
                availableForSelection: model.loggedIn && !model.isBusy;
                availableByHttp: model.availableByHttp;
                safeMode: model.safeMode;
                hasHdd: model.hasHdd;

                serverName: model.name;
                version: model.version;
                hardwareAddress: model.macAddress;
                displayAddress: model.ipAddress;
                os: model.os;

                operation: model.operation;

                onExplicitSelectionCalled: 
                {
                    thisComponent.tryChangeSelectedServers(function()
                    {
                        thisComponent.model.setItemSelected(index);
                    });
                }

                onSelectionStateShouldBeChanged:
                {
                    thisComponent.tryChangeSelectedServers( function()
                    {
                        thisComponent.model.changeItemSelectedState(index);
                    });
                }

                onUnauthrizedClicked: { thisComponent.model.blinkForItem(currentItemIndex); }

            }
        }

        Component
        {
            id: unknownGroupDeletate;

            Rtu.UnknownGroupDelegate
            {
                caption: model.name;
            }
        }

        Component
        {
            id: unknownItemDelegate;

            Rtu.UnknownItemDelegate
            {
                address: model.ipAddress;
            }
        }

        width: parent.width;

        sourceComponent:
        {
            switch(model.itemType)
            {
            case NxRtu.Constants.SystemItemType:
                return systemDelegate;
            case NxRtu.Constants.ServerItemType:
                return serverDelegate;
            case NxRtu.Constants.UnknownGroupType:
                return unknownGroupDeletate;
            case NxRtu.Constants.UnknownEntityType:
                return unknownItemDelegate;
            default:
                return undefined;
            }
        }
    }

    property QtObject impl: QtObject
    {
        readonly property int selectAllCheckedState:
            (!model.serversCount || !model.selectedCount ? Qt.Unchecked
            : (model.selectedCount === model.serversCount ? Qt.Checked : Qt.PartiallyChecked));

        function selectAllServers(select)
        {
            var selectedParam = select;
            thisComponent.tryChangeSelectedServers(function()
            {
                thisComponent.model.setAllItemSelected(selectedParam);
            });
        }
    }

    Dialogs.ErrorDialog
    {
        id: loginToSystemFailed;
        property string systemName;

        message: qsTr("Can't login to any server in system %1 with entered password").arg(systemName);

        Connections
        {
            target: rtuContext;
            onLoginOperationFailed:
            {
                loginToSystemFailed.systemName = primarySystem;
                loginToSystemFailed.show();
            }
        }
    }
}

