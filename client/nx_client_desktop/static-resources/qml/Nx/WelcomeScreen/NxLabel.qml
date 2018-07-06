import QtQuick 2.6
import Nx 1.0;

Text
{
    id: control;

    property bool acceptClicks: false;
    property bool autoHoverable: false;
    property bool isHovered: false;
    property bool disableable: true;

    property color standardColor: ColorTheme.windowText;
    property color hoveredColor: ColorTheme.lighter(standardColor, 1);
    property color disabledColor: standardColor;
    property var hoveredCursorShape: Qt.ArrowCursor;
    property real visibleTextWidth: visible
        ? fontMetrics.tightBoundingRect(control.text).width + leftPadding + rightPadding
        : 0

    signal clicked();

    opacity: (disableable && !enabled ? 0.3 : 1.0);

    color:
    {
        if (!enabled)
            return disabledColor;

        return (isHovered ? hoveredColor : standardColor);
    }

    height: 16;
    font.pixelSize: 13

    renderType: Text.QtRendering;
    verticalAlignment: Text.AlignVCenter;

    FontMetrics
    {
        id: fontMetrics
        font: control.font
    }


    Binding
    {
        target: control;
        property: "isHovered";
        when: control.autoHoverable;
        value: hoverArea.containsMouse;
    }

    MouseArea
    {
        id: hoverArea;

        anchors.fill: parent;
        acceptedButtons: (acceptClicks ? Qt.AllButtons : Qt.NoButton);
        hoverEnabled: control.autoHoverable;

        cursorShape: (control.isHovered
            ? control.hoveredCursorShape
            : Qt.ArrowCursor);

        onClicked: control.clicked();

        visible: (control.acceptClicks || control.autoHoverable) && (control.opacity > 0);
    }
}
