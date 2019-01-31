import QtQuick 2.1;
import QtQuick.Controls 1.1;

import "../common" as Common;
import "../controls/rtu" as Rtu;
import "../controls/base" as Base;
import "../controls/expandable" as Expandable;

import networkoptix.rtu 1.0 as Utils;

Expandable.MaskedSettingsPanel
{
    id: thisComponent;

    extraWarned: rtuContext.selection.safeMode;

    editBtnPrevTabIsSelf: true;
    activeFocusOnTab: false;
    changed:  (maskedArea && maskedArea.changed?  true : false);

    propertiesGroupName: qsTr("Configure Device Network Settings");

    onMaskedAreaChanged:
    {
        if (!warned || !maskedArea)
            return;

        var flagged = maskedArea.flaggedArea;
        var currentItem = flagged.currentItem;
        if (flagged && currentItem)
        {
            if (currentItem.enabled && currentItem.firstIpLine)
            {
                maskedArea.portNumberControl.KeyNavigation.backtab = currentItem.lastIpLine.useDHCPControl;
                if (currentItem.firstIpLine.useDHCPControl.checked)
                    currentItem.firstIpLine.dnsControl.forceActiveFocus();
                else
                    currentItem.firstIpLine.ipAddressControl.forceActiveFocus();

                return;
            }
        }

        maskedArea.portNumberControl.KeyNavigation.backtab = maskedArea.portNumberControl;
        maskedArea.portNumberControl.forceActiveFocus();
    }
    function tryApplyChanges(warnings)
    {
        if (!changed)
            return true;
        
        return maskedArea.tryApplyChanges(warnings);
    }

    propertiesDelegate: Component
    {
        id: test;

        Row
        {
            function tryApplyChanges(warnings)
            {
                if (portNumber.changed)
                    rtuContext.changesManager().changeset().addPortChange(Number(portNumber.text));

                if (!flagged.showFirst) // No changes
                    return true;

                return (flagged.currentItem.tryApplyChanges(warnings));
            }

            property bool changed: portNumber.changed || flagged.changed;
            property alias portNumberControl: portNumber;
            property alias flaggedArea: flagged ;
            spacing: Common.SizeManager.spacing.large;

            height: Math.max(flagged.height, portColumn.height);

            anchors
            {
                left: (parent ? parent.left : undefined);
                top: (parent ? parent.top : undefined);
                leftMargin: Common.SizeManager.spacing.base;
            }

            activeFocusOnTab: false;

            Rtu.FlaggedItem
            {
                id: flagged;

                property bool changed: (showFirst && currentItem.changed ? true : false);

                anchors
                {
                   top: (showFirst ? parent.top : undefined);
                   verticalCenter: (showFirst ? undefined : parent.verticalCenter);
                }

                message: qsTr("Can not change interfaces settings for some selected servers");
                showItem: rtuContext.selection.editableInterfaces;

                item: Rtu.IpSettingsList
                {
                    visible: (rtuContext.selection.count !== 0);
                    enabled: (Utils.RestApiConstants.AllowIfConfigFlag & rtuContext.selection.flags);
                    KeyNavigation.tab: portNumber;
                }
            }

            Base.Column
            {
                id: portColumn;

                Base.Text
                {
                    thin: false;
                    text: qsTr("Port");
                }

                Base.PortControl
                {
                    id: portNumber;

                    width: Common.SizeManager.clickableSizes.base * 4 ;
                    initialPort: rtuContext.selection.port;

                    KeyNavigation.tab: null;
                }
            }
        }
    }
}

