// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Core.Controls
import Nx.Controls
import Nx.Models

Item
{
    id: control

    property var systemId
    property var localId
    property bool cloudSystem: false
    property bool factorySystem: false
    property bool needDigestCloudPassword: false
    property bool running: false
    property string ownerDescription
    property bool reachable: false
    property bool compatible: true
    property bool wrongCustomization: false
    property string invalidVersion

    property var type: null
    property string title: ""
    property string text: ""
    property int counter: -1
    property bool editEnabled: false

    signal clicked()
    signal editClicked()

    height: 116

    SystemHostsModel
    {
        id: hostsModel
        systemId: `${control.systemId}`
        localSystemId: control.localId
    }

    ModelDataAccessor
    {
        id: hostsModelAccessor
        model: hostsModel

        property var defaultAddress: ""

        onDataChanged: (startRow) =>
        {
            if (startRow === 0)
                updateDefaultAddress()
        }

        onRowsMoved: updateDefaultAddress()
        onCountChanged: updateDefaultAddress()

        function updateDefaultAddress()
        {
            defaultAddress = count > 0
                ? getData(0, "url-internal")
                : NxGlobals.emptyUrl()
        }
    }

    Rectangle
    {
        width: parent.width
        height: parent.height
        x: 20
        color: ColorTheme.colors.dark5
        radius: 8

        visible: draggable.x < -1

        Popup
        {
            id: popup

            x: editButton.x
            y: editButton.y
            width: editButton.width
            height: editButton.height

            popupType: Popup.Item

            visible: parent.visible

            background: Rectangle
            {
                width: popup.width
                height: popup.height
                color: "transparent"
                opacity: 0.5
            }

            TapHandler
            {
                onSingleTapped:
                {
                    console.log("popup clicked")
                    control.editClicked()
                    popup.close()
                }
            }

            onClosed:
            {
                if (!dragHandler.active)
                    draggable.x = 0
            }
        }

        IconButton
        {
            id: editButton

            width: 48
            height: 48

            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: 20

            background: Rectangle
            {
                color: "transparent"
                radius: 4
                border.width: 1
                border.color: ColorTheme.colors.dark10
            }

            icon.source: "image://skin/20x20/Solid/edit_filled.svg?primary=light10"
            icon.width: 20
            icon.height: 20
        }
    }

    Item
    {
        id: draggable
        width: parent.width
        height: parent.height
        Behavior on x
        {
            enabled: !dragHandler.active
            NumberAnimation { duration: 100 }
        }

        Rectangle
        {
            anchors.fill: parent
            color: control.enabled ? ColorTheme.colors.dark8 : ColorTheme.colors.dark6
            radius: 8

            Item
            {
                anchors.fill: parent
                anchors.margins: 20

                Text
                {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top

                    elide: Text.ElideRight
                    textFormat: Text.StyledText

                    text: control.title
                    color: control.enabled ? ColorTheme.colors.light4 : ColorTheme.colors.dark15
                    font.pixelSize: 18

                    visible: control.type == OrganizationsModel.System
                }

                ColoredImage
                {
                    id: image
                    sourceSize: Qt.size(32, 32)
                    visible: control.type != OrganizationsModel.System
                    primaryColor: {
                        switch (control.type)
                        {
                            case OrganizationsModel.Folder:
                                return ColorTheme.colors.brand_core
                            case OrganizationsModel.Organization:
                                return ColorTheme.colors.light6
                            default:
                                return
                        }
                    }
                    secondaryColor: {
                        switch (control.type)
                        {
                            case OrganizationsModel.Folder:
                                return ColorTheme.colors.brand_d1
                            case OrganizationsModel.Organization:
                                return ColorTheme.colors.light10
                            default:
                                return
                        }
                    }
                    sourcePath: {
                        switch (control.type)
                        {
                            case OrganizationsModel.Folder:
                                return "image://skin/32x32/Solid/folder.svg"
                            case OrganizationsModel.Organization:
                                return "image://skin/32x32/Solid/organization.svg"
                            default:
                                return ""
                        }
                    }
                }

                RowLayout
                {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right

                    Text
                    {
                        id: secondaryText

                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignBaseline

                        horizontalAlignment: Text.AlignLeft
                        elide: Text.ElideRight
                        textFormat: Text.StyledText

                        text:
                        {
                            if (control.type == OrganizationsModel.System)
                            {
                                return control.ownerDescription
                                    || hostsModelAccessor.defaultAddress.displayAddress()
                            }
                            return control.text
                        }

                        color:
                        {
                            if (!control.enabled)
                                return ColorTheme.colors.dark16

                            return control.type == OrganizationsModel.System
                                ? ColorTheme.colors.light15
                                : ColorTheme.colors.light4
                        }

                        font.pixelSize: control.type == OrganizationsModel.System ? 12 : 18
                    }

                    Text
                    {
                        id: counterText
                        Layout.alignment: Qt.AlignBaseline
                        font.pixelSize: 14
                        color: ColorTheme.colors.light16
                        text: control.counter >= 0 ? `${control.counter}` : ""
                    }

                    IssueLabel
                    {
                        id: issueLabel

                        visible: text !== ""

                        Layout.alignment: Qt.AlignBaseline

                        color:
                        {
                            if (control.compatible)
                                return ColorTheme.colors.dark16

                            return wrongCustomization
                                ? ColorTheme.colors.red_core
                                : ColorTheme.colors.yellow_core
                        }

                        textColor: compatible ? ColorTheme.colors.dark6 : ColorTheme.colors.dark1

                        text:
                        {
                            if (control.type != OrganizationsModel.System)
                                return ""

                            if (!compatible)
                                return wrongCustomization ?  qsTr("INCOMPATIBLE") : invalidVersion

                            if (!running)
                                return qsTr("OFFLINE")

                            if (!reachable)
                                return qsTr("UNREACHABLE")

                            return ""
                        }
                    }
                }
            }

            TapHandler
            {
                onSingleTapped:
                {
                    control.clicked()
                }
                onLongPressed:
                {
                    if (control.editEnabled)
                        return

                    control.editClicked()
                }
            }

            DragHandler
            {
                id: dragHandler

                property real distance: 48+20

                enabled: control.editEnabled && false

                yAxis.enabled: false
                target: draggable
                xAxis.maximum: 0
                xAxis.minimum: -distance
                onActiveChanged:
                {
                    if (!active && target.x > -distance + 1)
                        target.x = 0
                }
            }
        }
    }
}
