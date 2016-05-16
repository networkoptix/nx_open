import QtQuick 2.2
import Nx 1.0
import com.networkoptix.qml 1.0

FocusScope {
    id: textField

    property alias placeholderText: placeholder.text
    property alias text: textInput.text
    property alias validator: textInput.validator
    property alias echoMode: textInput.echoMode
    property alias passwordMaskDelay: textInput.passwordMaskDelay
    property alias inputMethodHints: textInput.inputMethodHints
    property int leftPadding: dp(8)
    property int rightPadding: dp(8)
    property alias passwordCharacter: textInput.passwordCharacter
    property bool showError: false
    property alias showDecoration: decoration.visible

    property int textPadding: 0

    property color inactiveColor: showError ? ColorTheme.orange_d1 : ColorTheme.base10
    property color activeColor: showError ? ColorTheme.orange_main : ColorTheme.brand_main
    property color textColor: showError ? ColorTheme.orange_d1 : ColorTheme.windowText
    property color placeholderColor: ColorTheme.base10
    property color cursorColor: activeColor
    property alias selectionAllowed: textInput.selectionAllowed

    signal accepted()
    signal editingFinished()

    implicitHeight: dp(48)
    implicitWidth: Math.round(textInput.contentHeight * 8)

    activeFocusOnTab: true

    Item {
        id: decoration
        anchors.fill: parent
        anchors.margins: dp(4)

        Rectangle {
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            border.width: 0
            height: 1
            color: textInput.activeFocus ? activeColor : inactiveColor
        }

        Rectangle {
            anchors {
                left: parent.left
                bottom: parent.bottom
            }
            border.width: 0
            height: dp(6)
            width: dp(1)
            color: textInput.activeFocus ? activeColor : inactiveColor
        }

        Rectangle {
            anchors {
                right: parent.right
                bottom: parent.bottom
            }
            border.width: 0
            height: dp(6)
            width: dp(1)
            color: textInput.activeFocus ? activeColor : inactiveColor
        }
    }

    Item {
        anchors {
            fill: parent
            leftMargin: textPadding + leftPadding
            rightMargin: textPadding / 3 + rightPadding
        }

        QnTextInput {
            id: textInput
            focus: true

            width: parent.width
            anchors.verticalCenter: parent.verticalCenter

            color: textField.textColor
            cursorColor: textField.cursorColor
            font.pixelSize: sp(16)
            font.weight: Font.Normal
            renderType: TextInput.NativeRendering

            verticalAlignment: Text.AlignVCenter

            Keys.forwardTo: textField

            onAccepted: textField.accepted()
            onEditingFinished: textField.editingFinished()
        }

        Text {
            id: placeholder
            anchors.fill: parent
            font: textInput.font
            horizontalAlignment: textInput.horizontalAlignment
            verticalAlignment: textInput.verticalAlignment
            opacity: !textInput.text.length && !textInput.inputMethodComposing ? 1 : 0
            clip: contentWidth > width;
            elide: Text.ElideRight
            Behavior on opacity { NumberAnimation { duration: 90 } }
            color: placeholderColor
            renderType: Text.NativeRendering
        }
    }
}
