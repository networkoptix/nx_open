import QtQuick 2.4

pragma Singleton

QtObject {
    id: theme

    property color windowBackground
    property color windowText

    property color buttonBackground
    property color buttonText

    property color inputBorder
    property color inputBorderActive
    property color inputBorderError
    property color inputText
    property color inputTextError
    property color inputPlaceholder

    property color attentionBackground

    function loadColors() {
        windowBackground        = colorTheme.color("window.background")
        windowText              = colorTheme.color("window.text")
        buttonBackground        = colorTheme.color("button.background")
        buttonText              = colorTheme.color("button.text")
        inputBorder             = colorTheme.color("input.border")
        inputBorderActive       = colorTheme.color("input.borderActive")
        inputBorderError        = colorTheme.color("input.borderError")
        inputText               = colorTheme.color("input.text")
        inputTextError          = colorTheme.color("input.textError")
        inputPlaceholder        = colorTheme.color("input.placeholder")
        attentionBackground     = colorTheme.color("other.attentionBackground")
    }

    function transparent(color, opacity) {
        return Qt.rgba(color.r, color.g, color.b, opacity)
    }

    Component.onCompleted: loadColors()
}
