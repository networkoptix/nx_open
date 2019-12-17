import Nx.Controls 1.0

import "private"

LabeledItem
{
    id: control

    property alias value: textField.text
    property string defaultValue: ""
    property string validationRegex: ""
    property string validationRegexFlags: ""

    contentItem: TextField
    {
        readonly property var validationRegex:
            new RegExp(control.validationRegex, control.validationRegexFlags)

        id: textField
        text: defaultValue
        warningState: control.validationRegex && !validationRegex.test(text)
    }
}
