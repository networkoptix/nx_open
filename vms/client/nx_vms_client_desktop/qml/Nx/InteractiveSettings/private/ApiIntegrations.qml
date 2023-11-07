// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls

import nx.vms.client.core
import nx.vms.client.desktop

Scrollable
{
    id: root

    property var requestsModel: null

    contentItem: Column
    {
        id: column
        width: root.width - scrollBarWidth

        spacing: 16

        Section
        {
            header: qsTr("API Integrations")
            description:
                qsTr("API Integrations interact with %1 Server via REST API and exchange metadata")
                    .arg(Branding.vmsName())

            width: parent.width
        }

        Section
        {
            id: requestSection

            header: qsTr("New requests") + (requests.count > 0 ? ` (${requests.count})` : "")
            description: qsTr("API Integration may be enabled after verifying the code received"
                + " from Integration provider. New requests may be disabled to prevent unwanted"
                + " interaction.")

            visible: !!requestsModel
            checkable: false
            width: parent.width

            Binding on checked { value: requestsModel && requestsModel.isNewRequestsEnabled }
            onSwitchClicked: requestsModel.setNewRequestsEnabled(checked)
        }

        Repeater
        {
            id: requests

            property int count: model ? model.length : 0

            model: requestsModel && requestsModel.requests
            width: parent.width

            RequestPanel
            {
                property var id: request.requestId

                width: column.width
                request: modelData

                onApproveClicked: (authCode) =>
                {
                    const result = MessageBox.exec(
                        MessageBox.Icon.Question,
                        qsTr("Enable %1?").arg(request.name),
                        qsTr("This Integration will be able to:\n"
                            + " \u2022 change settings on cameras\n"
                            + " \u2022 get access to archive\n"
                            + " \u2022 process video streams\n"
                            + " \u2022 exchange metadata over the network"),
                        MessageBox.Ok | MessageBox.Cancel,
                        /*warningButton*/ null,
                        /*defaultButton*/ null,
                        qsTr("Enable Integration"));

                    if (result !== MessageBox.Cancel)
                        requestsModel.approve(id, authCode)
                }

                onRejectClicked:
                {
                    const result = MessageBox.exec(
                        MessageBox.Icon.Warning,
                        qsTr("Remove %1?").arg(request.name),
                        qsTr("This will remove Integration request"),
                        MessageBox.Cancel,
                        {
                            text: qsTr("Remove"),
                            role: MessageBox.AcceptRole
                        },
                        /*defaultButton*/ null,
                        qsTr("Remove Request"));

                    if (result !== MessageBox.Cancel)
                        requestsModel.reject(id)
                }
            }
        }
    }
}
