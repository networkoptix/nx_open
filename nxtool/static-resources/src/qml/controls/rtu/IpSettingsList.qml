import QtQuick 2.1;
import QtQuick.Controls 1.1;

import "." as Rtu;
import "../base" as Base;
import "../../common" as Common;
import "../../dialogs" as Dialogs;

    
Base.Column
{
    id: thisComponent;

    property Item firstIpLine: repeater.itemAt(0);
    property Item lastIpLine: repeater.itemAt(repeater.count - 1);

    Component.onCompleted:
    {
        firstIpLine = repeater.itemAt(0);
        if (firstIpLine && enabled)
            firstIpLine.ipAddressControl.forceActiveFocus();
    }

    property bool changed:
    {      
        var result = false;
        var children = thisComponent.children;
        for (var i = 0; i !== children.length; ++i)
        {
            var item = children[i];
            if (!item.hasOwnProperty("changed"))
                continue;
            
            result |= item.changed;
        }
        return result;
    }

    function tryApplyChanges (warnings) { return impl.tryApplyChanges(warnings); }
    
    anchors.left: (parent ? parent.left : undefined);

    activeFocusOnTab: false;

    Repeater
    {
        id: repeater;
        
        model: rtuContext.ipSettingsModel();
        
        delegate: Rtu.IpChangeLine
        {
            selectionSize: rtuContext.selection.count;
            useDHCPControl.initialCheckedState: useDHCP;

            ipAddressControl.initialText: model.address;
            subnetMaskControl.initialText: model.subnetMask;
            dnsControl.initialText: model.dns;
            gatewayControl.initialText: model.gateway;
            
            adapterNameValue: model.adapterName;
            interfaceCaption: model.readableName;
        }

        onItemAdded:
        {
            if (index !== 0)
            {
                var prevChild = itemAt(index - 1);
                item.KeyNavigation.backtab = (prevChild ? prevChild.dnsControl : null);
                prevChild.KeyNavigation.tab = item.ipAddressControl;
            }
            else
            {
                item.KeyNavigation.backtab = item.ipAddressControl;
                item.ipAddressControl.forceActiveFocus();
            }

            item.KeyNavigation.tab = thisComponent.KeyNavigation.tab;
        }
    }
    
    Dialogs.ErrorDialog
    {
        id: errorDialog;
    }
    
    ///

    property QtObject impl : QtObject
    {
        readonly property string errorTemplate: 
            qsTr("Invalid %1 for \"%2\" specified. Can't apply changes.");
        
        function tryApplyChanges(warnings)
        {
            if (!thisComponent.changed)
                return true;
        
            var children = thisComponent.children;
            for (var i = 0; i !== children.length; ++i)
            {
                var item = children[i];
                if (!item.hasOwnProperty("adapterNameValue")
                    || !item.hasOwnProperty("changed")
                    || !item.changed)
                {
                    continue;
                }

                var useRange = item.useIpRangeEnter;

                var name = item.adapterNameValue;
                var interfaceCaption = item.interfaceCaption;

                var useDHCPState = item.useDHCPControl.checkedState;
                var wrongGateway = (!item.gatewayControl.isEmptyAddress && !item.gatewayControl.acceptableInput);

                if (useDHCPState === Qt.Unchecked)   /// do not send address/mask/gateway if dhcp is on
                {
                    if (item.ipAddressControl.acceptableInput)
                    {
                        rtuContext.changesManager().changeset().addAddressChange(name, item.ipAddressControl.text);
                    }
                    else
                    {
                        errorDialog.message = errorTemplate.arg(qsTr("ip address")).arg(interfaceCaption);
                        errorDialog.show();
            
                        item.ipAddressControl.forceActiveFocus();
                        return false;
                    }

                    if (item.subnetMaskControl.acceptableInput
                        && rtuContext.isValidSubnetMask(item.subnetMaskControl.text))
                    {
                        rtuContext.changesManager().changeset().addMaskChange(name, item.subnetMaskControl.text);
                    }
                    else
                    {
                        errorDialog.message = errorTemplate.arg(qsTr("mask")).arg(interfaceCaption);
                        errorDialog.show();
                        
                        item.subnetMaskControl.forceActiveFocus();
                        return false;
                    }

                    if (!rtuContext.isDiscoverableFromCurrentNetwork(
                        item.ipAddressControl.text, item.subnetMaskControl.text))
                    {
                        warnings.push(("The IP address of \"%1\" is about to be assigned is in a different subnet. The unit will be unreachable after the changes are made. Proceed?")
                            .arg(interfaceCaption));
                    }

                    var gatewayDiscoverable = rtuContext.isDiscoverableFromNetwork(
                        item.ipAddressControl.text, item.subnetMaskControl.text
                        , item.gatewayControl.text, item.subnetMaskControl.text);

                    if (!wrongGateway && !item.gatewayControl.isEmptyAddress && !gatewayDiscoverable)
                    {
                        errorDialog.message = ("Default gateway for \"%1\" is not in the same network as IP. Can't apply changes.")
                            .arg(interfaceCaption);
                        errorDialog.show();

                        if (item.ipAddressControl.changed)
                            item.ipAddressControl.forceActiveFocus();
                        else
                            item.gatewayControl.forceActiveFocus();

                        return false;
                    }

                    if (item.gatewayControl.changed)
                    {
                        if (wrongGateway)
                        {
                            errorDialog.message = errorTemplate.arg(qsTr("gateway")).arg(interfaceCaption);
                            errorDialog.show();

                            item.gatewayControl.forceActiveFocus();
                            return false;
                        }
                        rtuContext.changesManager().changeset().addGatewayChange(name
                            , (item.gatewayControl.isEmptyAddress ? "" : item.gatewayControl.text));
                    }

                    /// TODO: add multiplse address processing
                }
                
                if (item.dnsControl.changed)
                {
                    if (!item.dnsControl.isEmptyAddress && !item.dnsControl.acceptableInput)
                    {
                        errorDialog.message = errorTemplate.arg(qsTr("dns")).arg(interfaceCaption);
                        errorDialog.show();
                        
                        item.dnsControl.forceActiveFocus();
                        return false;
                    }

                    rtuContext.changesManager().changeset().addDNSChange(name
                        , (item.dnsControl.isEmptyAddress ? "" : item.dnsControl.text));
                }

                if (item.useDHCPControl.changed)
                {
                    var useDHCP = (useDHCPState === Qt.Checked);
                    rtuContext.changesManager().changeset().addDHCPChange(name, useDHCP);
                }

            }
            return true;
        }
    }
    
    
}


