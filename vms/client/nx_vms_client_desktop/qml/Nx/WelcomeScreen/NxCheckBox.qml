import QtQuick 2.5
import QtQuick.Controls 2.4
import Nx 1.0
import nx.client.desktop 1.0

// TODO: add half-checked state (future)
// TODO: add table-specific skin (future)

CheckBox
{
    id: thisComponent;

    topPadding: 0
    bottomPadding: 0

    opacity: (enabled ? 1.0 : 0.3);

    signal accepted();

    Keys.onEnterPressed: { thisComponent.accepted(); }
    Keys.onReturnPressed: { thisComponent.accepted(); }

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

    contentItem: NxLabel
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

        FocusFrame
        {
            anchors.fill: parent
            anchors.leftMargin: -2
            anchors.rightMargin: -2
            visible: thisComponent.activeFocus
            color: ColorTheme.colors.brand_d4
        }
    }
}
