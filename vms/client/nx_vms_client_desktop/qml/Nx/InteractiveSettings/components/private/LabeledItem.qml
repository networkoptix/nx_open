// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

/**
 * Labeled Item with an explicit label (see LabeledColumnLayout).
 */
FocusScope
{
    property string name: ""
    property string description: ""
    property string caption: ""
    property string errorMessage: ""

    property alias contentItem: control.contentItem

    /** Explicit label text (see LabeledColumnLayout). */
    property string labelText: caption
    property bool isGroup: false

    // This property only affects defaultValueTooltipText() function return value.
    // Descendant components should define GlobalToolTip bindings where required.
    property bool defaultValueTooltipEnabled: false

    implicitWidth: control.implicitWidth
    implicitHeight: control.implicitHeight
    baselineOffset: control.baselineOffset

    Control
    {
        id: control
        anchors.fill: parent
    }

    function defaultValueTooltipText(value)
    {
        return defaultValueTooltipEnabled
            ? qsTr("Default value:") + " " + value
            : ""
    }
}
