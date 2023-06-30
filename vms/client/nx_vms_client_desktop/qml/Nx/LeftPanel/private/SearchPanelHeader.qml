// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14
import QtQuick.Controls 2.14

import Nx.Core 1.0
import Nx.Controls 1.0

import nx.vms.client.core 1.0
import nx.vms.client.desktop 1.0

Column
{
    id: header

    property RightPanelModel model: null
    property alias limitToCurrentCamera: cameraSelector.limitToCurrentCamera

    property alias filtersColumn: filtersColumn

    property alias searchText: searchField.text

    signal filtersReset()

    SearchField
    {
        id: searchField

        height: 28
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 8
        anchors.rightMargin: 8

        visible: !!d.textFilter
        placeholderTextColor: ColorTheme.windowText

        iconSource: ""
        leftPadding: 8

        background: Rectangle
        {
            radius: 2

            color:
            {
                if (searchField.activeFocus)
                    return ColorTheme.colors.dark3

                if (searchField.hovered)
                    return ColorTheme.colors.dark5

                return ColorTheme.colors.dark4
            }

            border.width: 1
            border.color: searchField.activeFocus || searchField.hovered
                ? ColorTheme.colors.dark7
                : ColorTheme.colors.dark6
        }
    }

    Control
    {
        id: filters

        padding: 8
        width: header.width

        contentItem: Column
        {
            id: filtersColumn
            spacing: 2

            TimeSelector
            {
                id: timeSelector
                setup: (header.model && header.model.commonSetup) || null
                width: Math.min(implicitWidth, filtersColumn.width)
            }

            CameraSelector
            {
                id: cameraSelector
                setup: (header.model && header.model.commonSetup) || null
                width: Math.min(implicitWidth, filtersColumn.width)
            }
        }
    }

    NxObject
    {
        id: d

        readonly property var textFilter: model && model.commonSetup.textFilter

        property string delayedText

        PropertyUpdateFilter on delayedText
        {
            source: searchField.text
            minimumIntervalMs: 250
        }

        Binding
        {
            target: d.textFilter
            property: "text"
            value: d.delayedText
            when: !!d.textFilter
        }

        Connections
        {
            target: d.textFilter
            ignoreUnknownSignals: !d.textFilter

            function onTextChanged()
            {
                searchField.text = d.textFilter.text
            }
        }

        Connections
        {
            target: model

            function onIsOnlineChanged()
            {
                if (model.isOnline)
                    return

                timeSelector.deactivate()
                cameraSelector.deactivate()
                searchField.clear()
                filtersReset()
            }
        }
    }
}
