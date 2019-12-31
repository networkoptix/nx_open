import QtQuick 2.0
import Nx 1.0

FocusScope
{
    property alias warningText: warningMessage.text
    property real warningSpacing: 8

    implicitWidth: textField.implicitWidth
    implicitHeight: textField.implicitHeight
        + (warningMessage.visible ? warningMessage.height + warningSpacing : 0)

    property alias textField: textField

    // Expose some frequently used properties to simplify the component usage. Other properties are
    // available via textField property.
    property alias text: textField.text
    property alias warningState: textField.warningState

    TextField
    {
        id: textField
        width: parent.width
    }

    Text
    {
        id: warningMessage

        anchors.top: textField.bottom
        anchors.topMargin: warningSpacing
        width: parent.width

        color: ColorTheme.colors.red_l2
        font.pixelSize: 14
        wrapMode: Text.WordWrap

        visible: textField.warningState && text
    }
}
