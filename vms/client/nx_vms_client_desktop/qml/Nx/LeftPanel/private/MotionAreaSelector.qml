// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14

import Nx.Core 1.0
import Nx.Controls 1.0

Item
{
    id: control

    property var motionModel

    height: motionAreaButton.height

    SelectableTextButton
    {
        id: motionAreaButton

        text: qsTr("In selected area")
        icon.source: "image://svg/skin/text_buttons/frame_20x20_deprecated.svg"
        selectable: false
        accented: true
        visible: state !== SelectableTextButton.Deactivated

        desiredState: motionModel.filterEmpty
            ? SelectableTextButton.Deactivated
            : SelectableTextButton.Unselected

        onDeactivated:
            motionModel.clearFilterRegions()
    }

    Text
    {
        id: motionAreaButtonPlaceholder

        text: qsTr("Select an area on the video to filter results")
        verticalAlignment: Text.AlignVCenter
        leftPadding: 6
        height: motionAreaButton.height
        color: ColorTheme.midlight
        visible: motionAreaButton.state === SelectableTextButton.Deactivated
    }
}
