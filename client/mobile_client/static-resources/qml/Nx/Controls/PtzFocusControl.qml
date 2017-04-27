import QtQuick 2.6

import Nx 1.0

import "private" as Private

Private.GenericValueControl
{
    id: control

    property alias supportsAutoFocus: control.showCenterArea
    property alias focusInPressed: control.upPressed
    property alias focusOutPressed: control.downPressed
    property bool autoFocusIsOn: false

    actionButtonHeight: control.supportsAutoFocus ? 52 : 56

    signal autoFocusClicked()

    upButtonDecoration: Image
    {
        anchors.centerIn: parent
        source: lp("/images/ptz/ptz_plus.png")
        opacity: enabled ? 1.0 : 0.2
    }

    downButtonDecoration: Image
    {
        anchors.centerIn: parent
        source: lp("/images/ptz/ptz_minus.png")
        opacity: enabled ? 1.0 : 0.2
    }

    centralAreaDelegate: Private.RoundButton
    {
        width: control.width
        height: 32

        Image
        {
            anchors.centerIn: parent
            source: control.autoFocusIsOn
                ? lp("/images/ptz/af_on.png")
                : lp("/images/ptz/af_off.png")

            opacity: enabled ? 1.0 : 0.2
        }

        onClicked: { control.autoFocusClicked() }
    }
}
