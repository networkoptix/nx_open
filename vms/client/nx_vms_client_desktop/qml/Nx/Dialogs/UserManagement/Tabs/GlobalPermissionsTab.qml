// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx
import Nx.Core
import Nx.Controls
import Nx.Items

import nx.vms.client.desktop

import "../Components"

Item
{
    id: control

    required property var model
    property bool editable: true

    ColumnLayout
    {
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.top: parent.top
        anchors.topMargin: 16

        spacing: 0

        Text
        {
            text: qsTr("On system level user has permissions to:")

            font: Qt.font({pixelSize: 14, weight: Font.Normal})
            color: ColorTheme.colors.light16
            bottomPadding: 16 - parent.spacing
        }

        Repeater
        {
            model: control.model

            delegate: RowLayout
            {
                Layout.leftMargin: -3
                spacing: 8

                CheckBox
                {
                    id: viewLogsCheckBox

                    enabled: control.editable

                    text: model.display

                    checkState: model.isChecked ? Qt.Checked : Qt.Unchecked
                    onCheckStateChanged: model.isChecked = (checkState == Qt.Checked)

                    Layout.alignment: Qt.AlignTop
                }

                IconImage
                {
                    id: inheritedFromIcon

                    property string toolTip: model.toolTip || ""
                    visible: !!toolTip

                    Layout.alignment: Qt.AlignTop
                    Layout.topMargin: viewLogsCheckBox.baselineOffset - baselineOffset
                    baselineOffset: height * 0.7 //< This is a property of this icon type.
                    sourceSize: Qt.size(20, 20)
                    source: "image://svg/skin/user_settings/sharing/group.svg"
                    color: groupMouseArea.containsMouse
                        ? ColorTheme.colors.light16
                        : ColorTheme.colors.dark13

                    MouseArea
                    {
                        id: groupMouseArea
                        hoverEnabled: true
                        anchors.fill: parent

                        GlobalToolTip.text: inheritedFromIcon.toolTip
                        GlobalToolTip.delay: 0
                    }
                }
            }
        }
    }
}
