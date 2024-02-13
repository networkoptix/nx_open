// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14
import QtQuick.Controls 2.14

import Nx.Controls 1.0
import nx.vms.client.core 1.0

DropdownMenu
{
    id: menu

    property var model: null

    function filterToString(filterValue)
    {
        switch (filterValue)
        {
            case WelcomeScreen.HiddenTileScopeFilter:
                return qsTr("Hidden")
            case WelcomeScreen.AllSystemsTileScopeFilter:
                return qsTr("All Sites")
            case WelcomeScreen.FavoritesTileScopeFilter:
                return qsTr("Favorites")
        }
        return "Invalid tile filter value"
    }

    onOpened:
    {
        for (let i = 0; i < count; ++i)
        {
            if (itemAt(i).checked)
                itemAt(i).forceActiveFocus()
        }
    }

    Action
    {
        id: actionAll

        text: menu.filterToString(WelcomeScreen.AllSystemsTileScopeFilter)
        checked: !model || model.visibilityFilter === WelcomeScreen.AllSystemsTileScopeFilter
        onTriggered: model.visibilityFilter = WelcomeScreen.AllSystemsTileScopeFilter
    }

    Action
    {
        id: actionFavorites

        text: menu.filterToString(WelcomeScreen.FavoritesTileScopeFilter)
        checked: model && model.visibilityFilter === WelcomeScreen.FavoritesTileScopeFilter
        onTriggered: model.visibilityFilter = WelcomeScreen.FavoritesTileScopeFilter
    }

    Action
    {
        id: actionHidden

        text: menu.filterToString(WelcomeScreen.HiddenTileScopeFilter)
        checked: model && model.visibilityFilter === WelcomeScreen.HiddenTileScopeFilter
        onTriggered: model.visibilityFilter = WelcomeScreen.HiddenTileScopeFilter
    }
}
