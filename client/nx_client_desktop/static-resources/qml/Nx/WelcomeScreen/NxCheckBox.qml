import QtQuick 2.5;
import Qt.labs.controls 1.0;
import Nx 1.0;

// TODO: add half-checked state (future)
// TODO: add table-specific skin (future)

CheckBox
{
    id: thisComponent;

    property bool hovered: hoverArea.containsMouse;

    height: label.height;

    opacity: (enabled ? 1.0 : 0.3);

    signal accepted();

    Keys.onEnterPressed: { thisComponent.accepted(); }
    Keys.onReturnPressed: { thisComponent.accepted(); }

    MouseArea
    {
        id: hoverArea;

        anchors.fill: parent;
        acceptedButtons: Qt.NoButton;
        hoverEnabled: true;
    }

    indicator: Image
    {
        width: 16;
        height: width;

        source:
        {
            if (thisComponent.hovered)
            {
                return (thisComponent.checked ? "qrc:/skin/theme/checkbox_checked_hover.png"
                    : "qrc:/skin/theme/checkbox_hover.png");
            }

            return (thisComponent.checked ? "qrc:/skin/theme/checkbox_checked.png"
                : "qrc:/skin/theme/checkbox.png");
        }
    }

    label: NxLabel
    {
        anchors.left: thisComponent.indicator.right;
        anchors.leftMargin: 6;
        text: thisComponent.text;
        enabled: thisComponent.enabled;

        isHovered: thisComponent.hovered;

        standardColor: thisComponent.checked ? ColorTheme.text : ColorTheme.light;
        hoveredColor: thisComponent.checked
            ? ColorTheme.brightText
            : ColorTheme.lighter(ColorTheme.light, 2);
        disabledColor: ColorTheme.light;

        Rectangle
        {
            // TODO: It is not possible to do dotted border. Make this in future
            anchors.fill: parent;
            visible: thisComponent.activeFocus;
            color: "transparent";
            border.color: ColorTheme.colors.brand_d4;
        }

    }
}
