// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core
import Nx.Core.Controls
import Nx.Mobile.Controls

import nx.vms.client.mobile

BaseBanner
{
    id: control

    property alias available: backend.available

    parent: Overlay.overlay

    anchors
    {
        top: parent.top
        left: parent.left
        right: parent.right
        topMargin: parent.SafeArea.margins.top
        leftMargin: parent.SafeArea.margins.left
        rightMargin: parent.SafeArea.margins.right
    }

    textColor: ColorTheme.colors.light1
    backgroundColor: ColorTheme.colors.dark6
    closeButtonColor: ColorTheme.colors.light17
    actionButtonColor: ColorTheme.colors.brand_core
    actionButtonFont.weight: Font.Medium
    borderColor: ColorTheme.colors.brand_core
    borderWidth: 2

    text: qsTr("Unlock exclusive features for actionable video intelligence and data-driven "
        + "operations at scale")

    iconItem: ColoredImage
    {
        sourcePath: "image://skin/20x20/Solid/rocket.svg"
        primaryColor: control.textColor
        sourceSize: Qt.size(20, 20)
    }

    action: Action
    {
        text: qsTr("Learn more")

        onTriggered: backend.openPromoUrl()
    }

    onCloseClicked: backend.dismiss()

    SaasPromoBackend
    {
        id: backend

        systemContext: windowContext.mainSystemContext
    }
}
