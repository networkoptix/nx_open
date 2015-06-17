import QtQuick 2.4

pragma Singleton

QtObject {
    id: theme

    property color textColor: colorTheme.color("nx_baseText")
    property color backgroundColor: colorTheme.color("nx_baseBackground")

    property color attentionColor: "#d65029"

}
