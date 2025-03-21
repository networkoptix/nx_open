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
    property bool isSaas: false
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

    readonly property bool tapEnabled: control.type != OrganizationsModel.System || running
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
        anchors.fill: parent
        color: control.tapEnabled ? ColorTheme.colors.dark8 : ColorTheme.colors.dark6
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
                color: control.tapEnabled ? ColorTheme.colors.light4 : ColorTheme.colors.dark16
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

                spacing: 4

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
                        if (!control.tapEnabled)
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

                ColoredImage
                {
                    id: cloudIcon

                    visible: control.cloudSystem && !control.isSaas

                    width: 20
                    height: 20

                    sourceSize: Qt.size(width, height)
                    sourcePath: "image://skin/20x20/Solid/cloud.svg"
                    primaryColor: ColorTheme.colors.brand_core
                    secondaryColor: ColorTheme.colors.brand_bg
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
                if (control.tapEnabled)
                    control.clicked()
            }
            onLongPressed:
            {
                if (control.editEnabled)
                    control.editClicked()
            }
        }
    }
}
