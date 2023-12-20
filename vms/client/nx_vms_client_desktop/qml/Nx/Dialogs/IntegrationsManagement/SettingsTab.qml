// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Controls

import Nx.InteractiveSettings

Control
{
    id: control

    property var requestsModel: null

    padding: 16

    contentItem: Section
    {
        header.text: qsTr("Accept API Integrations registration requests")
        checkable: true
        spacing: 4

        Binding on checked { value: requestsModel && requestsModel.isNewRequestsEnabled }
        onSwitchClicked: requestsModel.setNewRequestsEnabled(checked)
    }
}
