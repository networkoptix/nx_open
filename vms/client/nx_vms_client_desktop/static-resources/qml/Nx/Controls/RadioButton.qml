import QtQuick 2.6

CheckBox
{
    id: control

    autoExclusive: true

    indicator: Image
    {
        y: topPadding
        opacity: enabled ? 1.0 : 0.3

        source:
        {
            var source = "qrc:///skin/theme/radiobutton"
            if (control.checked)
                source += "_checked"
            if (control.hovered)
                source += "_hover"
            return source + ".png"
        }
    }
}
