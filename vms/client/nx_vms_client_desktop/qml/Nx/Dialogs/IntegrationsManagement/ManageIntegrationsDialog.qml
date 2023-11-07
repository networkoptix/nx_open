// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Window

import Nx.Core
import Nx.Controls
import Nx.Dialogs

DialogWithState
{
    id: dialog
    objectName: "ManageIntegrationsDialog"

    modality: Qt.ApplicationModal

    width: minimumWidth
    height: minimumHeight

    minimumWidth: 768
    minimumHeight: 700

    title: qsTr("Manage Integrations")
    color: ColorTheme.colors.dark7

    property bool loading: false
    property bool modified: false

    property var store: null

    DialogTabControl
    {
        anchors.fill: parent
        anchors.bottomMargin: buttonBox.height

        dialogLeftPadding: dialog.leftPadding
        dialogRightPadding: dialog.rightPadding

        Tab
        {
            button: DialogTabButton
            {
                text: qsTr("Integrations")
            }

            page: IntegrationsTab
            {
                store: dialog.store
                anchors.fill: parent
            }
        }
    }

    buttonBox: DialogButtonBox
    {
        buttonLayout: DialogButtonBox.KdeLayout
        standardButtons: DialogButtonBox.Ok | DialogButtonBox.Apply | DialogButtonBox.Cancel
    }
}
