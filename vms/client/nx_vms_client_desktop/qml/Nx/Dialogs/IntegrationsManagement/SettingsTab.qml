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
        text: qsTr("Enabling API Integrations registration requests allows third parties to "
            + "submit approval requests through integration. While safeguards exist, prolonged "
            + "usage is not recommended. Monitor usage closely and disable this option after all "
            + "necessary Integrations are installed.")

        Layout.fillWidth: true
    }
}
