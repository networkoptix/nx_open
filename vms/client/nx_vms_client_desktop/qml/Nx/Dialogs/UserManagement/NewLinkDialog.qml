// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts
import QtQuick.Window

import Nx.Core
import Nx.Controls
import Nx.Dialogs

import nx.vms.client.desktop

import ".."
import "Components"

Dialog
{
    id: dialog
    objectName: "newLinkDialog"

    modality: Qt.ApplicationModal

    minimumWidth: 620
    minimumHeight: content.implicitHeight + buttonBox.implicitHeight + 110 //< Adjust for calendar.

    width: minimumWidth
    height: minimumHeight

    title: qsTr("New Link - %1").arg(login)

    color: ColorTheme.colors.dark7

    property string login: ""
    property bool showWarning: true

    property alias linkValidUntil: temporaryLinkSettings.linkValidUntil
    property alias expiresAfterLoginS: temporaryLinkSettings.expiresAfterLoginS
    property alias revokeAccessEnabled: temporaryLinkSettings.revokeAccessEnabled

    property bool isSaving: false

    property var self

    function openNew()
    {
        dialog.show()
        dialog.raise()
    }

    Item
    {
        id: content

        implicitHeight: 16 * 2 + temporaryLinkSettings.height + banner.implicitHeight

        anchors.fill: parent
        anchors.bottomMargin: buttonBox.implicitHeight

        TemporaryLinkSettings
        {
            id: temporaryLinkSettings

            enabled: !dialog.isSaving

            x: 16
            y: 16
            width: parent.width - 16 * 2

            rightSideMargin: 100
            self: dialog.self
        }

        DialogBanner
        {
            id: banner

            style: DialogBanner.Style.Info
            closeable: true
            visible: dialog.showWarning && !closed

            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right

            text: qsTr("The previous link will be invalidated once a new one has been issued")
        }
    }

    function accept()
    {
        dialog.accepted()
        // Do not close the dialog, let the request complete.
    }

    buttonBox: DialogButtonBoxWithPreloader
    {
        id: buttonBox

        buttonLayout: DialogButtonBox.KdeLayout
        standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel
        preloaderButton: dialog.isSaving
            ? buttonBox.standardButton(DialogButtonBox.Ok)
            : undefined

        Component.onCompleted:
        {
            buttonBox.standardButton(DialogButtonBox.Ok).text = qsTr("Create")
        }
    }
}
