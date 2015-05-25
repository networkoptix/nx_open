import QtQuick 2.2

import com.networkoptix.qml 1.0

import "../common_functions.js" as CommonFunctions

FocusScope {
    id: textField

    property alias placeholderText: placeholder.text
    property alias text: textInput.text
    property alias validator: textInput.validator
    property alias echoMode: textInput.echoMode
    property alias inputMethodHints: textInput.inputMethodHints
    property int leftPadding
    property int rightPadding
    property alias passwordCharacter: textInput.passwordCharacter

    property int textPadding: 0

    property color inactiveColor: Qt.rgba(0, 0, 0, 0.3)
    property color activeColor: __syspal.button
    property alias textColor: textInput.color

    signal accepted()
    signal editingFinished()

    property var __syspal: SystemPalette {
        colorGroup: SystemPalette.Active
    }

    implicitWidth: Math.round(textInput.contentHeight * 8)
    implicitHeight: Math.max(25, Math.round(textInput.contentHeight * 1.2))

    activeFocusOnTab: true

    Rectangle {
        anchors {
            left: parent.left
            right: parent.right
        }
        y: parent.height
        border.width: 0
        height: textInput.activeFocus ? CommonFunctions.dp(2) : CommonFunctions.dp(1)
        color: textInput.activeFocus ? activeColor : inactiveColor
    }

    Item {
        anchors {
            fill: parent
            leftMargin: textPadding + leftPadding
            rightMargin: textPadding / 3 + rightPadding
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.IBeamCursor
            onClicked: textField.forceActiveFocus()
        }

        Flickable {
            id: flickable
            flickableDirection: Flickable.HorizontalFlick
            anchors.left: parent.left
            anchors.right: parent.right
            height: textInput.height
            y: Math.round((parent.height - textInput.height) / 2)

            interactive: contentWidth - 4 > width
            contentWidth: textInput.width + 2
            contentHeight: textInput.height
            clip: textInput.contentWidth > width - leftMargin - rightMargin
            boundsBehavior: Flickable.StopAtBounds

            TextInput {
                id: textInput
                focus: true
                selectByMouse: Qt.platform.os !== "android" // Workaround for QTBUG-36515

                verticalAlignment: Text.AlignVCenter
                clip: false

                Keys.forwardTo: textField
                autoScroll: false

                renderType: TextInput.NativeRendering

                onAccepted: {
                    Qt.inputMethod.commit()
                    Qt.inputMethod.hide()
                    textField.accepted()
                }

                onEditingFinished: textField.editingFinished()
                onCursorPositionChanged: {
                    if (!cursorVisible)
                        return

                    var rect = cursorRectangle
                    if (rect.x < flickable.contentX)
                        flickable.contentX = rect.x
                    var right = rect.x + rect.width
                    if (right > flickable.contentX + flickable.width)
                        flickable.contentX = right - flickable.width
                }
            }
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
            color: inactiveColor
            renderType: Text.NativeRendering
        }
    }
}
