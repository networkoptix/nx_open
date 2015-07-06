import QtQuick 2.0

import "../base" as Base;

Base.Column
{
    id: thisComponent;

    property int dhcpState: 0;
    property alias forceUseDHCP: forceUseDHCPChecklBox.checked;
    property alias changed: forceUseDHCPChecklBox.changed;
    
    function tryApplyChanges() 
    {
        if (!forceUseDHCPChecklBox.changed)
            return true;

        rtuContext.changesManager().addDHCPChange("doesn't matter for multiple selection", true);
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
        id: forceUseDHCPChecklBox;
        enabled: (thisComponent.dhcpState !== Qt.Checked);

        text: qsTr("Enable DHCP on all servers");
        initialCheckedState: Qt.Unchecked;
    }
}
