// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx
import Nx.Core
import Nx.Controls

import nx.vms.client.desktop

RowLayout
{
    id: header

    required property var engineInfo
    required property var licenseSummary

    property bool checkable: false
    property bool checked: false
    property var offline: undefined
    property bool refreshing: false
    property bool refreshable: false
    property bool removable: false

    signal enableSwitchClicked()
    signal refreshButtonClicked()
    signal removeClicked()

    implicitWidth: 100
    implicitHeight: 20
    spacing: 8

    SwitchIcon
    {
        id: enableSwitch

        visible: header.checkable
        hovered: mouseArea.containsMouse && !mouseArea.containsPress
        checkState: header.checked ? Qt.Checked : Qt.Unchecked
        Layout.alignment: Qt.AlignBaseline

        MouseArea
        {
            id: mouseArea

            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            hoverEnabled: true

            onClicked:
            {
                if (!header.checked
                    && engineInfo && engineInfo.isLicenseRequired
                    && licenseSummary.available <= licenseSummary.inUse)
                {
                    enableSwitch.checkState = Qt.Checked

                    const requiredIntegrationsCount =
                        licenseSummary.inUse - licenseSummary.available + 1
                    MessageBox.exec(
                        MessageBox.Icon.Critical,
                        qsTr("Insufficient services"),
                        qsTr(
                            "%n suitable integration service is required to turn on the integration",
                            "Required integration count",
                            requiredIntegrationsCount),
                        MessageBox.Ok)

                    enableSwitch.checkState =
                        Qt.binding(function() { return header.checked ? Qt.Checked : Qt.Unchecked })
                    return
                }

                header.enableSwitchClicked()
            }
        }
    }

    Text
    {
        id: name

        text: engineInfo ? engineInfo.name : ""
        color: ColorTheme.text
        font.pixelSize: 16
        font.weight: Font.Medium
        elide: Text.ElideRight
        Layout.alignment: Qt.AlignBaseline
    }

    Image
    {
        id: warning
        visible:
        {
            if (header.checkable && !header.checked)
                return false

            if (!engineInfo || !engineInfo.isLicenseRequired)
                return false

            return !!licenseSummary && licenseSummary.available < licenseSummary.inUse
        }
        source: "image://svg/skin/banners/warning.svg"
        sourceSize: Qt.size(20, 20)

        GlobalToolTip.text: licenseSummary && licenseSummary.issueExpirationDate
            ? qsTr("There are more cameras using this integration than available
                services. Please disable integration for some cameras or add more suitable services.
                Otherwise, it will be done automatically on %1", "%1 will be substituted by a date")
                    .arg(licenseSummary.issueExpirationDate)
            : ""
    }

    Text
    {
        id: extraInfo

        text:
        {
            if (!header.checkable || header.checked)
                return ""

            if (!engineInfo || !engineInfo.isLicenseRequired)
                return ""

            if (licenseSummary && licenseSummary.available > licenseSummary.inUse)
                return ""

            return qsTr("0 suitable services available")
        }
        color: ColorTheme.colors.red_core
        font.pixelSize: 14
        elide: Text.ElideRight
        visible: !!text
        Layout.alignment: Qt.AlignBaseline
    }

    Text
    {
        id: status

        text: qsTr("OFFLINE")
        visible: header.offline === true

        color: ColorTheme.colors.red_core
        font.weight: Font.Medium
        font.pixelSize: 10
        elide: Text.ElideRight
        Layout.alignment: Qt.AlignBaseline
    }

    Item
    {
        Layout.fillWidth: true
    }

    TextButton
    {
        id: refreshButton

        text: qsTr("Refresh")
        icon.source: "image://svg/skin/text_buttons/reload_20.svg"
        visible: header.refreshable && !header.refreshing

        onClicked:
            header.refreshButtonClicked()
    }

    Tag
    {
        id: tag

        visible: !header.checkable && !!engineInfo && !!engineInfo.isLicenseRequired
        text: qsTr("Services Required")
        textColor: ColorTheme.colors.light10
        color: ColorTheme.colors.dark7
        border.width: 1
        border.color: ColorTheme.colors.dark12
    }

    RowLayout
    {
        id: refreshingIndicator

        spacing: 2
        visible: header.refreshable && header.refreshing

        AnimatedImage
        {
            source: "qrc:///skin/legacy/loading.gif"
            Layout.alignment: Qt.AlignVCenter
        }

        Text
        {
            text: qsTr("Refreshing...")
            font: refreshButton.font
            color: refreshButton.color
            Layout.alignment: Qt.AlignVCenter
        }
    }

    TextButton
    {
        visible: header.removable
        text: qsTr("Remove")
        icon.source: "image://svg/skin/text_buttons/trash.svg"

        Layout.alignment: Qt.AlignBaseline
        onClicked:
        {
            header.removeClicked()
        }
    }
}
