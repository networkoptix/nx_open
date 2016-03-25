import QtQuick 2.5;
import QtQuick.Controls 1.3;
import QtQuick.Controls.Styles 1.3;

import "."

// TODO: add half-checked state (future)
// TODO: add table-specific skin (future)


CheckBox
{
    id: thisComponent;

    activeFocusOnTab: true;
    activeFocusOnPress: true;

    style: nxStyle;

    opacity: (enabled ? 1.0 : 0.3);

    Component
    {
        id: nxStyle;

        CheckBoxStyle
        {
            label: NxLabel
            {
                text: thisComponent.text;
                enabled: thisComponent.enabled;

                isHovered: control.hovered;

                standardColor: (control.checked ? Style.colors.text
                    : Style.colors.light);
                hoveredColor: (control.checked ? Style.colors.brightText
                    : Style.lighterColor(Style.colors.light, 2));
                disabledColor: Style.colors.light;

                Rectangle
                {
                    // TODO: It is not possible to do dotted border. Make this in future
                    anchors.fill: parent;
                    visible: control.activeFocus;
                    color: "#00000000";
                    border.color: Style.darkerColor(Style.colors.brand, 4);
                }
            }
        }
    }
}
