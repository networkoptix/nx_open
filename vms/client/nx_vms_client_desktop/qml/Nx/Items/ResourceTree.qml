import QtQuick 2.6

import Nx 1.0
import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

TreeView
{
    readonly property real kRowHeight: 20
    readonly property real kSeparatorRowHeight: 16

    indentation: 24
    scrollStepSize: kRowHeight
    hoverHighlightColor: ColorTheme.transparent(ColorTheme.colors.dark8, 0.4)
    selectionHighlightColor: ColorTheme.transparent(ColorTheme.colors.brand_core, 0.3)

    model: ResourceTreeModel {}

    delegate: Loader
    {
        readonly property bool selectable: !separator
        readonly property bool separator: (model && model.separator) || false

        sourceComponent: separator ? separatorComponent : normalRowComponent
        height: item && item.height

        Component
        {
            id: normalRowComponent

            Row
            {
                height: kRowHeight
                spacing: 4

                Image
                {
                    id: icon

                    width: 20
                    height: parent.height
                    fillMode: Image.Stretch
                    source: (model && model.iconKey && model.iconKey !== 0
                        && ("image://resource/" + model.iconKey)) || ""
                    visible: !separator
                }

                Text
                {
                    id: name

                    text: (model && model.display) || ""
                    color: ColorTheme.colors.light10
                    font.weight: Font.DemiBold
                    height: parent.height
                    verticalAlignment: Text.AlignVCenter
                    visible: !separator
                }

                Text
                {
                    id: extraInfo

                    text: (model && model.extraInfo) || ""
                    color: ColorTheme.colors.dark17
                    font.weight: Font.Normal
                    height: parent.height
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 1
                    visible: !separator
                }
            }
        }

        Component
        {
            id: separatorComponent

            Item
            {
                width: parent.width
                height: kSeparatorRowHeight

                Rectangle
                {
                    id: separatorLine

                    height: 1
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: -itemIndent

                    color: ColorTheme.transparent(ColorTheme.colors.dark8, 0.4)
                }
            }
        }
    }
}
