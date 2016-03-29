import QtQuick 2.6;
import QtQuick.Controls 1.4;
import QtQuick.Controls.Styles 1.4;

import "."

// TODO: left\right icons\btns implementation

TextField
{
    height: 28;

    property bool error; // TODO: improve logic: autoreset error when text changing

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
                    if (control.error)
                        return Style.colors.red_main;
                    if (control.readOnly)
                        return Style.colorWithAlpha(Style.colors.shadow, 0.4);
                    // TODO: ? add behavior of current non-focused item
//                    if (control.focus && !control.activeFocus)
//                        return Style.darkerColor(Style.colors.brand, 4);

                    return Style.darkerColor(color);
                }

                Rectangle
                {
                    id: topInnerShadow;

                    visible: (!control.readOnly && control.activeFocus)
                    anchors.top: parent.top;
                    height: 1;
                    x: 1;
                    width: parent.width - 2 * x;
                    color: Style.darkerColor(Style.colors.shadow, 3);
                }

                Rectangle
                {
                    id: rightInnerShadow;

                    visible: (!control.readOnly && control.activeFocus)
                    anchors.right: parent.right;
                    width: 1;
                    y: 1;
                    height: parent.height - 2 * y;
                    color: Style.darkerColor(Style.colors.shadow, 3);
                }
            }

            font: Qt.font({ pixelSize: 14
                , weight: (control.readOnly ? Font.Medium : Font.Normal) });
            textColor: Style.colors.text;

            placeholderTextColor: Style.colors.midlight;
            renderType: Text.QtRendering;
        }
    }
}
