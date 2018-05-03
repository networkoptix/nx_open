import QtQuick 2.6

Row
{
    id: control

    property alias primaryIndicator: primaryIndicator
    property alias secondaryIndicator: secondaryIndicator
    property real maxWidth: 0

    visible: opacity > 0
    onMaxWidthChanged: updateIndicatorsWidth()

    spacing: 2

    Indicator
    {
        id: secondaryIndicator

        visible: false
        onVisibleTextWidthChanged: updateIndicatorsWidth()
    }

    Indicator
    {
        id: primaryIndicator

        visible: false
        onVisibleTextWidthChanged: updateIndicatorsWidth()
    }

    function updateIndicatorsWidth()
    {
        var primaryWidth = primaryIndicator.visibleTextWidth
        var secondaryWidth = secondaryIndicator.visibleTextWidth
        var totalWidth = primaryWidth + secondaryWidth
        var aspect = totalWidth < maxWidth ? 1 : maxWidth / totalWidth
        primaryIndicator.maxWidth = primaryWidth * aspect
        secondaryIndicator.maxWidth = secondaryWidth * aspect
    }
}
