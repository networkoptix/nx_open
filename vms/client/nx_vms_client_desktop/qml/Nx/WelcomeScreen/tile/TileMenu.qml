// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4

import Nx.Models 1.0
import Nx.Controls 1.0
import nx.vms.client.core 1.0
import nx.vms.client.desktop 1.0

Menu
{
    id: menu

    property bool cloud: false
    property bool connectable: true
    property bool loggedIn: true
    property bool hideActionEnabled: true
    property var model: null

    signal editClicked()
    signal deleteClicked()
    signal tileHidden()

    modal:  false

    MenuItem
    {
        text: qsTr("Edit")

        icon.source: "image://skin/20x20/Outline/edit.svg"
        visible: (menu.model.tileType === ConnectTilesModel.GeneralTileType)
            && menu.model.isOnline
            && menu.loggedIn

        onTriggered: menu.editClicked()
    }

    MenuItem
    {
        text: menu.model.visibilityScope !== WelcomeScreen.HiddenTileVisibilityScope
            ? qsTr("Hide")
            : qsTr("Show")

        icon.source: menu.model.visibilityScope !== WelcomeScreen.HiddenTileVisibilityScope
            ? "image://skin/20x20/Outline/eye_closed.svg"
            : "image://skin/20x20/Outline/eye_open.svg"

        visible: menu.hideActionEnabled || menu.model.tileType === ConnectTilesModel.CloudButtonTileType

        onTriggered:
        {
            if (menu.model.visibilityScope !== WelcomeScreen.HiddenTileVisibilityScope)
            {
                if (!hideActionEnabled && menu.model.tileType === ConnectTilesModel.CloudButtonTileType)
                {
                    menu.close()
                    if (!context.confirmCloudTileHiding())
                        return
                }
                menu.model.visibilityScope = WelcomeScreen.HiddenTileVisibilityScope
                menu.tileHidden()
            }
            else
            {
                menu.model.visibilityScope = WelcomeScreen.DefaultTileVisibilityScope
            }
        }
    }

    MenuItem
    {
        text: qsTr("Delete")

        icon.source: "image://skin/20x20/Outline/delete.svg"

        visible: !menu.cloud && !menu.connectable

        onTriggered: deleteClicked()
    }

    MenuItem
    {
        text: menu.model.visibilityScope !== WelcomeScreen.FavoriteTileVisibilityScope
            ? qsTr("Add to Favorites")
            : qsTr("Remove from Favorites")

        icon.source: menu.model.visibilityScope !== WelcomeScreen.FavoriteTileVisibilityScope
            ? "image://skin/welcome_screen/tile/menu/star.svg"
            : "image://skin/welcome_screen/tile/menu/star_crossed.svg"

        visible: menu.model.tileType === ConnectTilesModel.GeneralTileType

        onTriggered:
        {
            menu.model.visibilityScope =
                menu.model.visibilityScope !== WelcomeScreen.FavoriteTileVisibilityScope
                    ? WelcomeScreen.FavoriteTileVisibilityScope
                    : WelcomeScreen.DefaultTileVisibilityScope
        }
    }
}
