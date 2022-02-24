// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0

Item
{
    property var highlights

    onHighlightsChanged:
    {
        rects.clear()
        if (!highlights)
            return
        for (let i = 0; i < highlights.length; i++)
        {
            rects.insert(i,
                {
                    rect: highlights[i].rect,
                    size: highlights[i].size,
                    name: highlights[i].name || "",
                    className: highlights[i].className || "",
                    location: highlights[i].location || "",
                })
        }
    }

    ListModel
    {
        id: rects
    }

    Repeater
    {
        id: repeater
        model: rects
        delegate: Item
        {
            Rectangle
            {
                id: selectionRectangle

                color: "transparent"
                border.color: "white"
                border.width: 1

                x: model.rect.x
                y: model.rect.y
                width: model.rect.width
                height: model.rect.height
            }

            Rectangle
            {
                radius: 2
                color: "white"
                width: sizeLabel.width + rectLabel.width + 6 + 6 + 6
                height: rectLabel.height + 8

                anchors.left: selectionRectangle.horizontalCenter
                anchors.leftMargin: -14
                anchors.top: selectionRectangle.bottom
                anchors.topMargin: 8

                Rectangle
                {
                    x: 5
                    width: 12
                    height: 12
                    color: "white"
                    transform: Rotation { origin.x: 0; origin.y: 0; angle: -45}
                }

                Text
                {
                    id: sizeLabel
                    anchors.verticalCenter: parent.verticalCenter
                    x: 6
                    color: "#303030"
                    text: `${model.size.width}x${model.size.height}`
                }

                Text
                {
                    id: rectLabel
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: sizeLabel.right
                    anchors.leftMargin: 6
                    text:
                    {
                        let text = `<span style="font-weight: bold; color:#0000a0;">${model.name}</span> <b>${model.className}</b>`
                        if (model.location)
                            text += `<br>${model.location}`
                        return text
                    }
                    textFormat: Text.RichText
                }
            }
        }
    }
}
