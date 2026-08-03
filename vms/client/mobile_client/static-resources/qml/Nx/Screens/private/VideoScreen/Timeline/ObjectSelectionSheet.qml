// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Controls
import Nx.Core
import Nx.Mobile.Controls

import nx.vms.client.mobile.timeline as Timeline

AdaptiveSheet
{
    id: sheet

    // MultiObjectData owns single object data pointers, while MultiObjectData.perObjectData doesn't.
    // So a copy of the entire MultiObjectData must be held here.
    // TODO: #vkutin Fix that ownership hazard in the future.
    property var /*Timeline.MultiObjectData*/ multiObjectData

    property int objectsType: Timeline.ObjectsLoader.ObjectsType.motion //< Determines icon color.
    property real tileHeight: 100
    property real tileSpacing: 8

    signal selected(int index)

    modal: true
    contentSpacing: 20

    ListView
    {
        id: listView

        width: parent.width

        height: Math.min(listView.contentHeight,
            sheet.availableContentHeight - cancelButton.height - sheet.contentSpacing)

        model: sheet.multiObjectData?.perObjectData ?? []
        spacing: sheet.tileSpacing
        clip: true

        delegate: Component
        {
            ObjectsListTile
            {
                id: tile

                height: sheet.tileHeight
                width: listView.width

                objectCount: 1
                highlighted: true
                showPointer: false

                caption: modelData?.title ?? ""
                description: modelData?.description ?? ""
                imagePaths: modelData?.imagePath ? [modelData.imagePath] : []
                iconPaths: modelData?.iconPath ? [modelData.iconPath] : []
                objectsType: sheet.objectsType

                MouseArea
                {
                    id: mouseArea

                    anchors.fill: tile

                    onClicked:
                        sheet.selected(index)
                }

                MaterialEffect
                {
                    id: rippleEffect

                    anchors.fill: parent
                    clip: true
                    radius: 6
                    mouseArea: mouseArea
                    rippleSize: 160
                    highlightColor: ColorTheme.transparent(ColorTheme.colors.light1, 0.3)
                }
            }
        }
    }

    Button
    {
        id: cancelButton

        text: qsTr("Cancel")
        type: Button.LightInterface
        width: parent.width

        onClicked:
            sheet.close()
    }
}
