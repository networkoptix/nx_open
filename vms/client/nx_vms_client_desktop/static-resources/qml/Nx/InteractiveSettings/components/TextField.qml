import Nx.Controls 1.0

import "private"

LabeledItem
{
    id: control

    property string defaultValue: ""
    property string validationRegex: ""
    property string validationRegexFlags: ""
    property string validationErrorMessage: ""

    signal valueChanged()

    readonly property bool filled: textField.text !== ""

    contentItem: TextFieldWithWarning
    {
        readonly property var validationRegex:
            new RegExp(control.validationRegex, control.validationRegexFlags)

        id: textField
        text: defaultValue
        warningText: validationErrorMessage

        textField.onEditingFinished: updateWarning()
        onTextChanged: valueChanged()

        function updateWarning()
        {
            warningState = validationRegex && !validationRegex.test(text)
        }
    }

    function getValue()
    {
        return textField.text
    }

    function setValue(value)
    {
        textField.text = value
        textField.updateWarning()
    }
}
