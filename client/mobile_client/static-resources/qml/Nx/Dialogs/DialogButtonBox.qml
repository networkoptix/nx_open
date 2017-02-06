import QtQuick 2.6
import QtQml 2.2

Row
{
    id: buttonBox

    property alias buttonsModel: instantiator.model

    signal buttonClicked(string buttonId)

    height: 48
    width: parent ? parent.width : implicitWidth

    Instantiator
    {
        id: instantiator

        model: [ "OK" ]

        DialogButton
        {
            parent: buttonBox
            text: modelData.text ? modelData.text : _standardButtonName(modelData)
            onClicked: buttonBox.buttonClicked(modelData.id)
        }
    }

    readonly property var _standardButtonNames:
    {
        "OK": qsTr("OK"),
        "CANCEL": qsTr("Cancel"),
        "CLOSE": qsTr("Close"),
        "YES": qsTr("Yes"),
        "NO": qsTr("No"),
        "ABORT": qsTr("Abort"),
        "RETRY": qsTr("Retry")
    }

    function _standardButtonName(modelData)
    {
        if (!modelData)
            return ""

        var id = modelData.id ? modelData.id : modelData
        var text = _standardButtonNames[id]
        return text ? text : id
    }
}
