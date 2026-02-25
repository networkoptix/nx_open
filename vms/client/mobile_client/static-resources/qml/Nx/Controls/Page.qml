// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls as QuickControls
import Nx.Controls
import Nx.Core
import Nx.Ui

import nx.vms.client.mobile

QuickControls.Page
{
    id: control

    property bool activePage: StackView.status === StackView.Active
    property int pageStatus: StackView.status
    property var customBackHandler
    property string result
    property bool titleUnderlineVisible: false

    property alias leftButtonIcon: toolBar.leftButtonIcon
    property alias leftButtonImageSource: toolBar.leftButtonImageSource
    property alias leftButtonEnabled: toolBar.leftButtonEnabled
    property alias titleControls: toolBar.controls
    property alias warningText: warningPanel.text
    property alias warningVisible: warningPanel.opened
    property alias toolBar: toolBar
    property alias warningPanel: warningPanel
    property alias titleLabelOpacity: toolBar.titleOpacity
    property alias backgroundColor: backgroundRectangle.color
    property alias gradientToolbarBackground: toolBar.useGradientBackground

    property alias centerControl: toolBar.centerControl
    property alias rightControl: toolBar.rightControl

    signal leftButtonClicked()
    signal headerClicked()

    clip: true

    header: Item
    {
        implicitWidth: column.implicitWidth
        implicitHeight: column.implicitHeight

        Column
        {
            id: column
            width: parent.width
            bottomPadding: 1

            ToolBar
            {
                id: toolBar
                title: control && control.title
                titleUnderlineVisible: control.titleUnderlineVisible
                leftButtonIcon.source: stackView.depth > 1 ? d.kBackButtonIconSource : ""
                onLeftButtonClicked: control.leftButtonClicked()
                onClicked: control.headerClicked()
            }

            WarningPanel
            {
                id: warningPanel
            }
        }

        Rectangle
        {
            anchors.bottom: parent.bottom
            height: 1
            width: parent.width
            visible: toolBar.visible || warningPanel.opened
            color: ColorTheme.colors.dark7 //< Keep in sync with main window background color.
        }
    }

    background: Rectangle
    {
        id: backgroundRectangle

        color: ColorTheme.colors.dark4
    }

    Keys.onPressed: (event) =>
    {
        if (!CoreUtils.keyIsBack(event.key))
            return

        event.accepted = true

        if (control.customBackHandler)
            control.customBackHandler(event.key === Qt.Key_Escape)
        else if (leftButtonIcon.source === d.kBackButtonIconSource && stackView.depth > 1)
            Workflow.popCurrentScreen()
    }

    QtObject
    {
        id: d

        readonly property url kBackButtonIconSource: lp("/images/arrow_back.png")
    }
}
