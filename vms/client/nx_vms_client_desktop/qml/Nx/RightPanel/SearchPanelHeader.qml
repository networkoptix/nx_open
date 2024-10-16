// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Controls

import nx.vms.client.core
import nx.vms.client.desktop

import "private"

Column
{
    id: header

    property RightPanelModel model: null
    property alias limitToCurrentCamera: cameraSelector.limitToCurrentCamera

    property alias filtersColumn: filtersColumn

    property alias searchText: searchField.text
    property int searchDelay: 250

    topPadding: 8
    bottomPadding: 8
    spacing: 8

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

    // Do not remove this Control wrapper.
    // It separates inner layout from possible outer layouts.
    // It adds paddings, making sizing in external children added to `filterColumn` easier.
    Control
    {
        id: filters

        leftPadding: 8
        rightPadding: 8
        width: header.width

        // Do not change this to Column, it is bugged and causes binding loops.
        contentItem: ColumnLayout
        {
            id: filtersColumn
            spacing: 2

            TimeSelector
            {
                id: timeSelector
                setup: (header.model && header.model.commonSetup) || null
                Layout.maximumWidth: filtersColumn.width
            }

            CameraSelector
            {
                id: cameraSelector
                setup: (header.model && header.model.commonSetup) || null
                Layout.maximumWidth: filtersColumn.width
                limitToCurrentCamera: header.model.crossSiteMode
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
            minimumIntervalMs: header.searchDelay
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
                header.filtersReset()
            }
        }
    }

    Component.onCompleted:
        filtersReset()

    onFiltersReset:
    {
        timeSelector.deactivate()
        searchField.clear()
    }
}
