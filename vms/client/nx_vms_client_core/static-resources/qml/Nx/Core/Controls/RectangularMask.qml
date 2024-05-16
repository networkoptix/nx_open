// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14
import QtQuick.Shapes 1.14

Shape
{
    id: shape

    property rect rectangle: Qt.rect(0, 0, 1, 1) //< Normalized.

    property rect absoluteRect: Qt.rect(
        rectangle.x * width,
        rectangle.y * height,
        rectangle.width * width,
        rectangle.height * height)

    property alias color: path.fillColor

    clip: true

    ShapePath
    {
        id: path

        strokeWidth: -1

        PathPolyline
        {
            path: [
                Qt.point(0, 0),
                Qt.point(shape.width, 0),
                Qt.point(shape.width, shape.height),
                Qt.point(0, shape.height),
                Qt.point(0, 0)]
        }

        PathPolyline
        {
            path: [
                Qt.point(absoluteRect.left, absoluteRect.top),
                Qt.point(absoluteRect.right, absoluteRect.top),
                Qt.point(absoluteRect.right, absoluteRect.bottom),
                Qt.point(absoluteRect.left, absoluteRect.bottom),
                Qt.point(absoluteRect.left, absoluteRect.top)]
        }
    }
}
