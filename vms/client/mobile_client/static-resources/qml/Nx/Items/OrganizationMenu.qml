// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Controls
import Nx.Core
import Nx.Mobile.Controls
import Nx.Ui

Item
{
    id: control

    property string name
    readonly property bool opened: loader.item?.visible ?? false
    readonly property bool anchored: opened && loader.sourceComponent === menuComponent
    property bool expandedLayout: LayoutController.isExpanded

    signal structureClicked()
    signal logInClicked()

    function open(anchorItem)
    {
        loader.sourceComponent = expandedLayout ? menuComponent : sheetComponent

        if (expandedLayout)
            loader.item.popup(anchorItem, -loader.item.width, 0)
        else
            loader.item.open()
    }

    function close()
    {
        if (loader.item)
            loader.item.close()
    }

    component BetaLabel: IssueLabel
    {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter

        text: qsTr("BETA")
        color: ColorTheme.colors.yellow_core
        textColor: ColorTheme.colors.dark1
    }

    Action
    {
        id: structureAction

        text: qsTr("Organization Structure")

        onTriggered:
        {
            control.close()
            control.structureClicked()
        }
    }

    Action
    {
        id: logInAction

        text: qsTr("Log In")

        onTriggered:
        {
            control.close()
            control.logInClicked()
        }
    }

    Loader
    {
        id: loader
    }

    Component
    {
        id: menuComponent

        Menu
        {
            minimumWidth: 320

            MenuItem
            {
                id: structureMenuItem

                action: structureAction
            }

            MenuSeparator {}

            MenuItem
            {
                id: logInMenuItem

                action: logInAction

                BetaLabel
                {
                    anchors.rightMargin: parent.rightPadding
                }
            }
        }
    }

    Component
    {
        id: sheetComponent

        AdaptiveSheet
        {
            id: sheet

            title: control.name
            titleTextItem.wrapMode: Text.NoWrap
            titleTextItem.elide: Text.ElideRight
            preferredEdge: Qt.BottomEdge

            Button
            {
                id: structureButton

                width: parent.width
                type: Button.LightInterface
                action: structureAction
                textHorizontalAlignment: Text.AlignLeft
            }

            Button
            {
                id: logInButton

                width: parent.width
                type: Button.LightInterface
                action: logInAction
                textHorizontalAlignment: Text.AlignLeft

                BetaLabel
                {
                    anchors.rightMargin: parent.rightPadding
                }
            }

            footer: Button
            {
                id: cancelButton

                text: qsTr("Cancel")
                type: Button.LightInterface

                onClicked: sheet.close()
            }
        }
    }
}
