// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls
import Nx.Mobile.Controls

import nx.vms.client.mobile.timeline as Timeline

BottomSheet
{
    id: sheet

    property int selectedType: Timeline.ObjectsLoader.ObjectsType.motion

    title: qsTr("View")
    contentSpacing: 0

    component ObjectsTypeRadioButton: RadioButton
    {
        required property int objectsType

        leftPadding: 0
        rightPadding: 0

        onClicked:
        {
            sheet.selectedType = objectsType
            sheet.close()
        }
    }

    ObjectsTypeRadioButton
    {
        id: motionsButton

        text: qsTr("Motions")
        icon.source: "image://skin/24x24/Outline/motion.svg"
        objectsType: Timeline.ObjectsLoader.ObjectsType.motion
        width: parent.width
        checked: true
    }

    ObjectsTypeRadioButton
    {
        id: bookmarksButton

        text: qsTr("Bookmarks")
        icon.source: "image://skin/24x24/Outline/bookmark.svg"
        objectsType: Timeline.ObjectsLoader.ObjectsType.bookmarks
        width: parent.width
    }

    ObjectsTypeRadioButton
    {
        id: objectsButton

        text: qsTr("Objects")
        icon.source: "image://skin/24x24/Outline/object.svg"
        objectsType: Timeline.ObjectsLoader.ObjectsType.analytics
        width: parent.width
    }

    Item
    {
        width: parent.width
        height: 20
    }

    Button
    {
        text: qsTr("Close")
        type: Button.LightInterface
        width: parent.width

        onClicked:
            sheet.close()
    }

    onSelectedTypeChanged:
    {
        for (let item of sheet.data)
        {
            if (item instanceof ObjectsTypeRadioButton && item.objectsType === selectedType)
            {
                item.checked = true
                break
            }
        }
    }
}
