import QtQuick 2.1;

import "../base" as Base;
import "../../common" as Common;

Grid
{
    id: thisComponent;

    property bool changed: (useDHCPCheckBox.changed || ipControlField.changed
        || subnetMaskField.changed || dnsControl.changed || gatewayControl.changed);

    property int selectionSize: 0;

    readonly property bool useIpRangeEnter: (selectionSize > 1);
    readonly property alias useDHCPControl: useDHCPCheckBox;
    readonly property alias ipAddressControl: ipControlField;
    readonly property alias subnetMaskControl: subnetMaskField;
    readonly property alias dnsControl: dnsServerField;
    readonly property alias gatewayControl: defaultGatewayField;
    readonly property alias lastIpContol: lastAddressContol;

    property string adapterNameValue;
    property string interfaceCaption;

    spacing: Common.SizeManager.spacing.base;

    verticalItemAlignment: Grid.AlignVCenter;

    columns: 3;

    activeFocusOnTab: false;

    Base.Text
    {
        id: adapterName;

        thin: false;
        text: (useIpRangeEnter ? qsTr("Static IP Range") : interfaceCaption);
    }

    Base.EmptyCell {}   /// Address column holder

    Base.EmptyCell {}   /// Use DHCP column holder

    ///
    Base.Text
    {
        id: ipAddressText;

        text: (useIpRangeEnter ? qsTr("Start IP") : qsTr("IP"));
    }

    Base.IpControl
    {
        id: ipControlField

        enabled: (useDHCPCheckBox.checkedState == Qt.Unchecked);

        KeyNavigation.tab: subnetMaskField;
        KeyNavigation.backtab: thisComponent.KeyNavigation.backtab;
    }

    Base.CheckBox
    {
        id: useDHCPCheckBox;

        activeFocusOnPress: true;
        activeFocusOnTab: true;

        height: subnetMaskField.height;
        text: qsTr("Use DHCP");

        KeyNavigation.tab: thisComponent.KeyNavigation.tab;
        KeyNavigation.backtab: dnsServerField;
    }

    ///

    /// row for range ip entering

    Base.Text
    {
        id: lastAddressText;
        visible: useIpRangeEnter;

        text: qsTr("End IP (auto)");
    }

    Base.IpEndRange
    {
        id: lastAddressContol;

        visible: useIpRangeEnter;
        addressesCount: selectionSize;
        startAddress: ipControlField.text;
    }

    Base.EmptyCell /// Use DHCP column holder
    {
        visible: useIpRangeEnter;
    }

    ///

    Base.Text
    {
        id: subnetMaskCaption;

        text: qsTr("Subnet mask");
    }


    Base.IpControl
    {
        id: subnetMaskField;

        enabled: (useDHCPCheckBox.checkedState == Qt.Unchecked);

        KeyNavigation.tab: defaultGatewayField;
        KeyNavigation.backtab: ipControlField;
    }

    Base.EmptyCell {}   /// Use DHCP column holder

    ///

    Base.Text
    {
        id: defaultGatewayText;

        text: qsTr("Default Gateway");
    }

    Base.IpControl
    {
        id: defaultGatewayField;

        enabled: (useDHCPCheckBox.checkedState == Qt.Unchecked);

        KeyNavigation.tab: dnsServerField;
        KeyNavigation.backtab: subnetMaskField;
    }

    Base.EmptyCell {}   /// Use DHCP column holder

    ///

    Base.Text
    {
        id: dnsServerText;

        text: qsTr("DNS server");
    }

    Base.IpControl
    {
        id: dnsServerField;

        KeyNavigation.tab: useDHCPCheckBox;
        KeyNavigation.backtab: (defaultGatewayField.enabled
            ? defaultGatewayField : thisComponent.KeyNavigation.backtab);
    }

    Base.EmptyCell {}   /// Use DHCP column holder
}
