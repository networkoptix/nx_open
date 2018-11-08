import QtQuick 2.0
import Nx.Controls 1.0

import "private"

LabeledItem
{
    id: control

    property alias value: checkBox.checked
    property bool defaultValue: false

    contentItem: CheckBox
    {
        id: checkBox
        checked: defaultValue
    }
}
