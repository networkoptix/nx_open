// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Common
import Nx.Core
import Nx.Core.Controls
import Nx.Mobile
import Nx.Mobile.Controls
import Nx.Ui

Sheet
{
    id: control

    title: qsTr("Layouts")

    Repeater
    {
        id: layoutsList

        width: parent.width
        clip: true

        model: windowContext.sessionManager.hasActiveSession ? layoutsModel : undefined

        delegate: AbstractButton
        {
            id: layoutItem

            property Resource layoutResource: model.resource
            property bool isCurrent: windowContext.depricatedUiController.layout
                ? windowContext.depricatedUiController.layout === layoutResource
                : model.itemType === QnLayoutsModel.AllCameras

            height: 48
            width: parent.width

            text: resourceName

            contentItem: Rectangle
            {
                width: parent.width
                height: parent.height
                radius: 4
                color: layoutItem.isCurrent
                    ? ColorTheme.colors.dark9
                    : ColorTheme.colors.dark7

                RowLayout
                {
                    x: 20
                    width: parent.width - x - 20
                    height: parent.height

                    ColoredImage
                    {
                        id: layoutIcon

                        Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter

                        primaryColor: layoutItem.isCurrent
                            ? ColorTheme.colors.light1
                            : ColorTheme.colors.light10
                        secondaryColor: layoutItem.isCurrent
                            ? ColorTheme.colors.light1
                            : ColorTheme.colors.light4

                        sourcePath:
                        {
                            if (model.itemType === QnLayoutsModel.AllCameras)
                                return "image://skin/20x20/Solid/cameras.svg"

                            if (model.itemType == QnLayoutsModel.CloudLayout)
                                return "image://skin/20x20/Solid/layout_cloud.svg"

                            return model.shared
                                ? "image://skin/20x20/Solid/layout_shared.svg"
                                : "image://skin/20x20/Solid/layout.svg"
                        }

                        sourceSize: Qt.size(20, 20)
                    }

                    Text
                    {
                        id: layoutNameText

                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter

                        text: layoutItem.text
                        color: layoutItem.isCurrent
                            ? ColorTheme.colors.light1
                            : ColorTheme.colors.light10
                        elide: Text.ElideRight

                        font.pixelSize: 16
                        font.weight: 400
                    }

                    Text
                    {
                        id: itemsCountText

                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter

                        text: model.itemsCount > 0
                            ? model.itemsCount
                            : ""

                        color: layoutNameText.color
                        font: layoutNameText.font
                    }
                }
            }

            onClicked:
            {
                Workflow.openResourcesScreen()
                windowContext.depricatedUiController.layout = layoutResource
                control.close()
            }
        }
    }

    QnLayoutsModel { id: layoutsModel }
}
