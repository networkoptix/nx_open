import QtQuick 2.6;
import QtQuick.Controls 1.4;
import QtQuick.Controls.Styles 1.4;

ComboBox
{
    id: thisComponent;

    focus: true;
    onActiveFocusChanged:
    {
        if (activeFocus)
            focus = true;
    }


    // TODO: Fix style to prevent font artefacts
}
