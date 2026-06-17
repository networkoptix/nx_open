// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls
import Nx.Core
import Nx.Mobile.Controls

import nx.vms.client.mobile.timeline as Timeline

AdaptiveSheet
{
    id: sheet

    property int selectedType: Timeline.ObjectsLoader.ObjectsType.motion

    signal objectsTypeClicked()

    title: qsTr("View")
    contentSpacing: 8

    component ObjectsTypeRadioButton: StyledRadioButton
    {
        required property int objectsType

        height: 44
        width: parent.width

        backgroundRadius: 8
        backgroundColor: ColorTheme.colors.dark12
        checkedBackgroundColor: ColorTheme.colors.dark14
        backgroundBorderWidth: checked ? 1 : 0
        backgroundBorderColor: ColorTheme.colors.dark18

        topPadding: 0
        bottomPadding: 0

        checkedTextColor: ColorTheme.colors.light4

        indicator: null

        font.weight: Font.Medium

        onClicked:
        {
            sheet.selectedType = objectsType
            sheet.objectsTypeClicked()
            sheet.close()
        }
    }

    ObjectsTypeRadioButton
    {
        id: motionsButton

        text: qsTr("Motion")
        icon.source: "image://skin/24x24/Outline/motion.svg?primary=light10"
        objectsType: Timeline.ObjectsLoader.ObjectsType.motion
        width: parent.width
        checked: true
    }

    ObjectsTypeRadioButton
    {
        id: bookmarksButton

        text: qsTr("Bookmarks")
        icon.source: "image://skin/24x24/Outline/bookmark.svg?primary=light10"
        objectsType: Timeline.ObjectsLoader.ObjectsType.bookmarks
        width: parent.width
    }

    ObjectsTypeRadioButton
    {
        id: objectsButton

        text: qsTr("Objects")
        icon.source: "image://skin/24x24/Outline/object.svg?primary=light10"
        objectsType: Timeline.ObjectsLoader.ObjectsType.analytics
        width: parent.width
    }

    footer: Button
    {
        text: qsTr("Cancel")
        type: Button.LightInterface

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
