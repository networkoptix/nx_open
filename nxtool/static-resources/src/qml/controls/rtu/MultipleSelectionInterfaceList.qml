import QtQuick 2.0

import "../base" as Base;

Base.Column
{
    id: thisComponent;

    property int dhcpState: 0;
    property alias forceUseDHCPControl: forceUseDHCPCheckBox;
    property alias forceUseDHCP: forceUseDHCPCheckBox.checked;
    property alias changed: forceUseDHCPCheckBox.changed;
    function tryApplyChanges(warnings)
    {
        if (!forceUseDHCPCheckBox.changed)
            return true;

        rtuContext.changesManager().changeset().addDHCPChange("doesn't matter for multiple selection", true);
        return true;
    }

    Base.Text
    {
        wrapMode: Text.Wrap;

        anchors.left: (parent ? parent.left : undefined);

        color: "red";
        text: qsTr("Ip address and subnet mask cannot be changed for multiple servers");
    }

    Base.CheckBox
    {
        enabled: false;

        text: qsTr("DHCP");
        checkedState: dhcpState;
    }

    Base.CheckBox
    {
        id: forceUseDHCPCheckBox;
        enabled: (thisComponent.dhcpState !== Qt.Checked);

        activeFocusOnTab: true;
        activeFocusOnPress: true;

        KeyNavigation.backtab: forceUseDHCPCheckBox;

        text: qsTr("Enable DHCP on all servers");
        initialCheckedState: Qt.Unchecked;
    }
}
