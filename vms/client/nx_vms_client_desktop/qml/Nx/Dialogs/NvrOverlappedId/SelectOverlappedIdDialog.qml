// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.11
import QtQml 2.14

import Nx 1.0
import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Dialogs 1.0

import nx.vms.client.desktop 1.0

Dialog
{
    id: control

    title: "Overlapped ID"

    property var model: null
    property int currentId: -1
    readonly property string filter: searchEdit.text

    width: minimumWidth
    height: minimumHeight
    minimumWidth: 300
    minimumHeight: 400

    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0

    contentItem: Rectangle
    {
        id: backgroundRect

        color: ColorTheme.colors.dark7

        Rectangle
        {
            id: labelRect

            width: parent.width
            height: nameLabel.contentHeight + nameLabel.anchors.topMargin
                + nameLabel.anchors.bottomMargin

            color: ColorTheme.colors.dark8

            Label
            {
                id: nameLabel

                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                anchors.topMargin: 12
                anchors.bottomMargin: 12

                font.pixelSize: 13
                color: ColorTheme.colors.light10
                wrapMode: Text.WordWrap

                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter

                text: qsTr(
                    "Timeline identifier, which is created when time is changed backward "
                    + "on a remote NVR. The new timeline can overlap the existing one.")
            }
        }

        Item
        {
            x: 12
            y: labelRect.height + 12
            width: parent.width - x - 12
            height: parent.height - y - 12

            Column
            {
                spacing: 12

                anchors.fill: parent

                SearchField
                {
                    id: searchEdit

                    width: parent.width
                }

                Row
                {
                    width: parent.width
                    height: parent.height - y

                    spacing: 10

                    Flickable
                    {
                        id: table

                        width: parent.width
                            - (scrollBar.visible ? scrollBar.width + parent.spacing : 0)
                        height: parent.height

                        clip: true
                        contentWidth: width
                        contentHeight: radioButtonColumn.height

                        flickableDirection: Flickable.VerticalFlick
                        boundsBehavior: Flickable.StopAtBounds
                        interactive: true

                        ScrollBar.vertical: scrollBar

                        Column
                        {
                            id: radioButtonColumn

                            spacing: 0

                            Repeater
                            {
                                id: repeater

                                model: control.model

                                delegate: Component
                                {
                                    StyledRadioButton
                                    {
                                        width: searchEdit.width

                                        text: model.display

                                        checked: control.currentId == model.id
                                        onToggled: control.currentId = model.id
                                    }
                                }
                            }
                        }
                    }

                    ScrollBar
                    {
                        id: scrollBar

                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.right: parent.right

                        policy: ScrollBar.AlwaysOn

                        visible: table.height < table.contentHeight
                    }
                }
            }
        }
    }

    buttonBox: DialogButtonBox
    {
        Button
        {
            text: qsTr("Select")
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            isAccentButton: true
        }
        Button
        {
            text: qsTr("Cancel")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
    }
}
