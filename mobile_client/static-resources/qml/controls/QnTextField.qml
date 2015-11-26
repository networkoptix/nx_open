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
    property int leftPadding: dp(8)
    property int rightPadding: dp(8)
    property alias passwordCharacter: textInput.passwordCharacter
    property bool showError: false
    property alias showDecoration: decoration.visible

    property int textPadding: 0

    property color inactiveColor: showError ? QnTheme.inputBorderError : QnTheme.inputBorder
    property color activeColor: showError ? QnTheme.inputBorderActiveError : QnTheme.inputBorderActive
    property color textColor: showError ? QnTheme.inputTextError : QnTheme.inputText
    property color placeholderColor: showError ? QnTheme.inputPlaceholderError : QnTheme.inputPlaceholder
    property color cursorColor: activeColor

    signal accepted()
    signal editingFinished()

    height: dp(48)

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
            height: textInput.activeFocus ? CommonFunctions.dp(1) : CommonFunctions.dp(1)
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

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.IBeamCursor
            onClicked: textField.forceActiveFocus()
        }

        QnFlickable {
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

                color: textField.textColor

                verticalAlignment: Text.AlignVCenter
                clip: false

                Keys.forwardTo: textField
                autoScroll: false

                renderType: TextInput.NativeRendering

                onAccepted: textField.accepted()

                font.pixelSize: sp(16)
                font.weight: Font.Normal

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

                cursorDelegate: Rectangle {
                    id: cursor
                    width: dp(1)
                    color: textField.cursorColor

                    Timer {
                        interval: 500
                        running: textInput.cursorVisible
                        repeat: true
                        onTriggered: cursor.visible = !cursor.visible
                        onRunningChanged: cursor.visible = running
                    }
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
            color: placeholderColor
            renderType: Text.NativeRendering
        }
    }
}
