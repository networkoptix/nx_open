// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Controls

ColumnLayout
{
    id: control

    property var store: null

    CheckBox
    {
        text: qsTr("Accept API Integrations registration requests")

        Binding on checked { value: store && store.isNewRequestsEnabled }
        onToggled: store.isNewRequestsEnabled = checked

        Layout.margins: 16
    }

    Item { Layout.fillHeight: true }

    DialogBanner
    {
        id: banner

        style: DialogBanner.Style.Warning
        visible: !closed && !!store && store.isNewRequestsEnabled
        watchToReopen: !!store && store.isNewRequestsEnabled
        closeable: true

        text: qsTr("Enabling this option lets third parties request approval to register an "
            + "Integration. Safeguards are in place, but disable it once all necessary "
            + "Integrations are installed - extended use is not recommended.")

        Layout.fillWidth: true
    }
}
