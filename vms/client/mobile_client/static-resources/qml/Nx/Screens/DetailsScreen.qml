// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Items
import Nx.Ui

import nx.vms.client.mobile.timeline as Timeline

import "private/DetailsScreen"

AdaptiveScreen
{
    id: root

    objectName: "detailsScreen"

    required property int objectsType
    required property var objectList

    title: detailsItem.title

    menuButton
    {
        visible: detailsItem.menu !== null
        onClicked: detailsItem.menu.popup(menuButton, 0, menuButton.height)
    }

    contentItem: DetailsItem
    {
        id: detailsItem

        property int currentIndex: 0

        objectsType: root.objectsType
        objectData: root.objectList[currentIndex]
        resource: root.objectList[currentIndex].resource
        showPreviewImage: objectsType === Timeline.ObjectsLoader.ObjectsType.analytics
        gestureExclusionEnabled: LayoutController.isPortrait && root.isActive

        hasNext: currentIndex < root.objectList.length - 1
        hasPrevious: currentIndex > 0

        onNextClicked: currentIndex = MathUtils.bound(0, currentIndex + 1, objectList.length - 1)
        onPreviousClicked: currentIndex = MathUtils.bound(0, currentIndex - 1, objectList.length - 1)
        onShowOnCameraRequested: Workflow.popCurrentScreen()
        onBackClicked: Workflow.popCurrentScreen()
    }
}
