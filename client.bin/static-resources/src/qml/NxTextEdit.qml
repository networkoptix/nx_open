import QtQuick 2.6;
import QtQuick.Controls 1.4;
import QtQuick.Controls.Styles 1.4;

import "."

// TODO: ? paddings 8px left/right
// TODO: left\right icons\btns implementation

TextField
{
    height: 28;

    property bool error;

    opacity: (enabled ? 1.0 : 0.3);

    style: nxTextEditStyle;

    Component
    {
        id: nxTextEditStyle;  // TODO: move in separate file

        TextFieldStyle
        {
            background: Rectangle
            {
                color:
                {
                    if (control.readOnly)
                        return Style.colorWithAlpha(Style.colors.shadow, 0.2);

                    if (control.activeFocus)
                        return Style.darkerColor(Style.colors.shadow);

                    return Style.colors.shadow;
                }

                radius: 1;
                border.width: 1;
                border.color:
                {
                    if (readOnly)
                        return Style.colorWithAlpha(Style.colors.shadow, 0.4);
                    if (control.focus && !control.activeFocus)
                        return Style.colors.brand;

                    return Style.darkerColor(color);
                }

                Rectangle
                {
                    id: topInnerShadow;

                    visible: (!control.readOnly && control.activeFocus)
                    anchors.top: parent.top;
                    height: 2;
                    color: Style.darkerColor(Style.colors.shadow, 3);
                }

                Rectangle
                {
                    id: rightInnerShadow;

                    visible: (!control.readOnly && control.activeFocus)
                    anchors.right: parent.right;
                    width: 2;
                    color: Style.darkerColor(Style.colors.shadow, 3);
                }
            }

            font: Qt.font({ pixelSize: 14
                , weight: (control.readOnly ? Font.Medium : Font.Normal) });
            textColor: Style.colors.text;

            placeholderTextColor: Style.colors.midlight;
        }
    }
}
