import QtQuick 2.1;

import "../base" as Base;
import "../../common" as Common;

Grid
{
    id: thisComponent;

    property bool changed: (useDHCPCheckBox.changed || ipControlField.changed
        || subnetMaskField.changed || dnsControl.changed || gatewayControl.changed);

    readonly property alias useDHCPControl: useDHCPCheckBox;
    readonly property alias ipAddressControl: ipControlField;
    readonly property alias subnetMaskControl: subnetMaskField;
    readonly property alias dnsControl: dnsServerField;
    readonly property alias gatewayControl: defaultGatewayField;

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
        text: interfaceCaption;
    }

    Base.EmptyCell {}

    Base.EmptyCell {}

    ///
    Base.Text
    {
        id: ipAddressText;

        text: qsTr("IP");
    }

    Base.IpControl
    {
        id: ipControlField

        enabled: !useDHCPCheckBox.checked;

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

    Base.Text
    {
        id: subnetMaskCaption;

        text: qsTr("Subnet mask");
    }


    Base.IpControl
    {
        id: subnetMaskField;

        enabled: !useDHCPCheckBox.checked;

        KeyNavigation.tab: defaultGatewayField;
        KeyNavigation.backtab: ipControlField;
    }

    Base.EmptyCell {}

    ///

    Base.Text
    {
        id: defaultGatewayText;

        text: qsTr("Default Gateway");
    }

    Base.IpControl
    {
        id: defaultGatewayField;

        enabled: !useDHCPCheckBox.checked;

        KeyNavigation.tab: dnsServerField;
        KeyNavigation.backtab: subnetMaskField;
    }

    Base.EmptyCell {}

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

    Base.EmptyCell {}
}
