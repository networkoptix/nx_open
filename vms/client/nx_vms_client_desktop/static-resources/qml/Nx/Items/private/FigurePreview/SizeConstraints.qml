import QtQuick 2.0

import Nx 1.0

Item
{
    id: figure

    property var figureJson: null

    property alias minBoxColor: minBox.baseColor
    property alias maxBoxColor: maxBox.baseColor

    readonly property bool hasFigure: true //< Figure is always defined.

    SizeConstraintsRectangle
    {
        id: maxBox

        originJson: figureJson && Array.isArray(figureJson.positions) && figureJson.positions[1]
        sizeJson: figureJson && figureJson.maximum
        baseColor: ColorTheme.colors.roiMaximum
        defaultSize: 1
    }

    SizeConstraintsRectangle
    {
        id: minBox

        originJson: figureJson && Array.isArray(figureJson.positions) && figureJson.positions[0]
        sizeJson: figureJson && figureJson.minimum
        baseColor: ColorTheme.colors.roiMinimum
        defaultSize: 0
    }
}
