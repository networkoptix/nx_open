import QtQuick 2.2
import QtQuick.Controls.Styles 1.2

import com.networkoptix.qml 1.0

import "../../common_functions.js" as CommonFunctions

TextFieldStyle {
    textColor: colorTheme.color("inputText")
    placeholderTextColor: colorTheme.color("inputPlaceholderText")
    background: Item {
        Rectangle {
            anchors {
                bottom: parent.bottom
                left: parent.left
                right: parent.right
                leftMargin: 6
                rightMargin: 6
                bottomMargin: -4
            }
            border.width: 0
            height: CommonFunctions.dp(2)
            color: control.activeFocus ? colorTheme.color("inputBorderActive") : colorTheme.color("inputBorder")
        }
    }
}
