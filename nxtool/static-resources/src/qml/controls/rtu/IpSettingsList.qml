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
        
        model: rtuContext.selection.itfSettingsModel;
        
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
        readonly property string errorTemplateServeral:
            qsTr("Invalid %1 specified. Can't apply changes.");

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

                if ((useDHCPState === Qt.Unchecked) &&   /// do not send address/mask/gateway if dhcp is on
                        (item.ipAddressControl.changed || item.subnetMaskControl.changed
                         || item.gatewayControl.changed || item.useDHCPControl.changed))
                {
                    var selectedCount = rtuContext.selection.count;
                    var isSingleSelection = (selectedCount == 1);
                    var ipAddressChangeAdded = false;

                    if (item.ipAddressControl.acceptableInput
                        && (!item.lastIpContol.visible || item.lastIpContol.valid))   /// for ranges
                    {
                        if (item.ipAddressControl.changed)  /// add address only if it was changed
                        {
                            rtuContext.changesManager().changeset().addAddressChange(name, item.ipAddressControl.text);
                            ipAddressChangeAdded = true;
                        }
                    }
                    else if (isSingleSelection   /// for single selection empty or wrong ip is not correct anyway

                        /// If there are no non-assigned addresses and current ip control value is empty
                        /// - we use old addresses (does not change ips)
                        || (!item.ipAddressControl.isEmptyAddress && (rtuContext.selection.hasEmptyIps || !item.lastIpContol.valid)))
                    {
                        if (isSingleSelection)
                            errorDialog.message = errorTemplate.arg(qsTr("ip address")).arg(interfaceCaption);
                        else
                            errorDialog.message = errorTemplateServeral.arg(qsTr("range of addresses"));

                        errorDialog.show();
            
                        item.ipAddressControl.forceActiveFocus();
                        return false;
                    }

                    var maskText = item.subnetMaskControl.text;
                    var validMask = rtuContext.isValidSubnetMask(maskText);

                    /// we have to check if start and finish addresses from the range are in the the same subnet
                    var maskValidWidth = isSingleSelection || item.ipAddressControl.isEmptyAddress
                        || (validMask && item.ipAddressControl.changed&& rtuContext.isDiscoverableFromNetwork(
                             item.ipAddressControl.text, item.subnetMaskControl.text, item.lastIpContol.text, item.subnetMaskControl.text));

                    if (item.subnetMaskControl.acceptableInput && validMask && maskValidWidth)
                    {
                        if (item.subnetMaskControl.changed) /// add subnet mask only if it was changed
                            rtuContext.changesManager().changeset().addMaskChange(name, item.subnetMaskControl.text);
                    }
                    else
                    {
                        if (!validMask)
                        {
                            if (isSingleSelection)
                                errorDialog.message = errorTemplate.arg(qsTr("mask")).arg(interfaceCaption);
                            else
                                errorDialog.message = errorTemplateServeral.arg(qsTr("mask"));
                        }
                        else if (!maskValidWidth)
                        {
                            errorDialog.message = qsTr("Subnet mask is too narrow. Servers are going to be in different subnets. Can't apply changes");
                        }
                        errorDialog.show();
                        
                        item.subnetMaskControl.forceActiveFocus();
                        return false;
                    }

                    var startIpDiscoverable = rtuContext.isDiscoverableFromCurrentNetwork(
                        item.ipAddressControl.text, item.subnetMaskControl.text);
                    var lastIpDiscoverable = (isSingleSelection || item.lastIpContol.valid && rtuContext.isDiscoverableFromCurrentNetwork(
                        item.lastIpContol.text, item.subnetMaskControl.text));

                    var showDiffSubnetWarning = (!startIpDiscoverable || !lastIpDiscoverable);

                    if (ipAddressChangeAdded && showDiffSubnetWarning)
                    {
                        if (isSingleSelection)
                            warnings.push(("IP address from a different subnet is about to be assigned to \"%1\". The device will be unreachable. Proceed?").arg(interfaceCaption));
                        else
                            warnings.push("IP addresses from a different subnet are about to be assigned to some devices. Such devices will be unreachable. Proceed?");
                    }

                    var gatewayDiscoverableFromStartIp = rtuContext.isDiscoverableFromNetwork(
                        item.ipAddressControl.text, item.subnetMaskControl.text
                        , item.gatewayControl.text, item.subnetMaskControl.text);
                    var gatewayDiscoverableFromLastIp = (isSingleSelection || rtuContext.isDiscoverableFromNetwork(
                        item.lastIpContol.text, item.subnetMaskControl.text
                        , item.gatewayControl.text, item.subnetMaskControl.text));

                    var gatewayDiscoverable = (gatewayDiscoverableFromStartIp && gatewayDiscoverableFromLastIp);
                    if (!wrongGateway && !item.gatewayControl.isEmptyAddress && !gatewayDiscoverable
                        && (isSingleSelection || !item.ipAddressControl.isEmptyAddress))
                    {
                        if (isSingleSelection)
                        {
                            errorDialog.message = ("Default gateway for \"%1\" is not in the same network as IP. Can't apply changes.")
                                .arg(interfaceCaption);
                        }
                        else
                            errorDialog.message = ("Default gateway is not in the same network as IPs. Can't apply changes.")

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
                            if (isSingleSelection)
                                errorDialog.message = errorTemplate.arg(qsTr("gateway")).arg(interfaceCaption);
                            else
                                errorDialog.message = errorTemplateServeral.arg(qsTr("gateway"));

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
                        if (isSingleSelection)
                            errorDialog.message = errorTemplate.arg(qsTr("dns")).arg(interfaceCaption);
                        else
                            errorDialog.message = errorTemplateServeral.arg(qsTr("dns"));

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


