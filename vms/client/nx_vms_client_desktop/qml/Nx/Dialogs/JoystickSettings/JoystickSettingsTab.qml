// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Core 1.0
import Nx.Controls 1.0

import nx.vms.client.core 1.0
import nx.vms.client.desktop 1.0

Item
{
    id: control

    readonly property int leftTabPadding: 16

    property bool basicMode: true
    property alias infoBarVisible: infoBar.visible

    implicitWidth: sliderLayout.minItemWidth * 2 + sliderLayout.spacing +
        2 * sliderLayout.x

    property JoystickButtonSettingsModel model: null
    property JoystickButtonActionChoiceModel buttonBaseActionChoiceModel: null
    property JoystickButtonActionChoiceModel buttonModifiedActionChoiceModel: null
    property FilteredResourceProxyModel layoutModel: null

    property alias panAndTiltHighlighted: panItem.highlighted
    property alias zoomHighlighted: zoomItem.highlighted

    Item
    {
        id: mainLayout

        anchors.fill: parent

        HintItem
        {
            id: hint

            x: control.leftTabPadding
            y: 12
            width: parent.width - x * 2

            visible: !control.basicMode

            buttonName: control.model ? control.model.modifierButtonName : ""
        }

        Row
        {
            id: sliderLayout

            readonly property int itemWidth: (width - spacing) / 2
            readonly property int minItemWidth:
                Math.max(panItem.contentWidth, zoomItem.contentWidth, selectItem.contentWidth)

            x: control.leftTabPadding
            y: 16 + (hint.visible ? hint.y + hint.height : 0)
            width: parent.width - x * 2
            height: panItem.height

            spacing: 15

            SensitivityItem
            {
                id: panItem

                width: parent.itemWidth

                visible: control.basicMode

                text: qsTr("Pan and Tilt")
                image: "image://svg/skin/joystick_settings/panAndTilt.svg"

                value: control.model ? control.model.panAndTiltSensitivity : 0

                onMoved:
                {
                    if (control.model)
                        control.model.panAndTiltSensitivity = value
                }
            }

            SensitivityItem
            {
                id: zoomItem

                width: parent.itemWidth

                visible: control.basicMode && control.model && control.model.zoomIsEnabled

                text: qsTr("Zoom In / Zoom Out")
                image: "image://svg/skin/joystick_settings/zoom.svg"

                value: control.model ? control.model.zoomSensitivity : 0

                onMoved:
                {
                    if (control.model)
                        control.model.zoomSensitivity = value
                }
            }

            SelectItem
            {
                id: selectItem

                width: parent.itemWidth
                height: zoomItem.height

                visible: !control.basicMode
                highlighted: panItem.highlighted

                text: qsTr("Select Camera on Layout")
                image: "image://svg/skin/joystick_settings/panAndTilt.svg"
            }
        }

        TableView
        {
            id: table

            anchors.top: sliderLayout.bottom
            anchors.topMargin: 24
            anchors.bottom: infoBar.visible ? infoBar.bottom : parent.bottom

            width: parent.width - control.leftTabPadding

            columnSpacing: 8
            rowSpacing: 8

            clip: true
            flickableDirection: Flickable.VerticalFlick
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: scrollBar

            model: control.model

            columnWidthProvider: function(column)
            {
                if (control.basicMode
                    && (column === JoystickButtonSettingsModel.ModifiedActionColumn
                    || column === JoystickButtonSettingsModel.ModifiedActionParameterColumn))
                {
                    return 0 //< Hide modified action and parameter columns.
                }

                if (!control.basicMode
                    && (column === JoystickButtonSettingsModel.BaseActionColumn
                    || column === JoystickButtonSettingsModel.BaseActionParameterColumn))
                {
                    return 0 //< Hide basic action and parameter columns.
                }

                return -1
            }

            delegate: ButtonSettingsDelegate
            {
                buttonActionChoiceModel: basicMode
                    ? control.buttonBaseActionChoiceModel
                    : control.buttonModifiedActionChoiceModel

                layoutModel: control.layoutModel

                basicMode: control.basicMode

                columnIndex: column
            }
        }

        AlertBar
        {
            id: infoBar

            anchors.bottom: parent.bottom

            info: true

            visible: false

            label.text: qsTr("Log in to the system to configure how to open layouts")
        }
    }

    ScrollBar
    {
        id: scrollBar

        anchors.right: parent.right
        y: table.y
        height: table.height

        policy: ScrollBar.AlwaysOn

        visible: table.height < table.contentHeight
    }
}
