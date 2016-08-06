import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0
import com.networkoptix.qml 1.0

import "private/LiteClientControlScreen"

Page
{
    id: liteClientControlScreen
    objectName: "liteClientControlScreen"

    property string clientId

    leftPadding: 8
    rightPadding: 8

    leftButtonIcon: lp("/images/menu.png")
    onLeftButtonClicked: sideNavigation.open()

    titleControls:
    [
        Switch
        {
            id: enabledSwitch

            onCheckedChanged:
            {
                if (!checked)
                    liteClientController.stopLiteClient()
            }
        }
    ]

    QtObject
    {
        id: d

        property Item screenItem:
        {
            if (liteClientControlScreen.state != "SingleCamera"
                && liteClientControlScreen.state != "MultipleCameras")
            {
                return null
            }
            return screenLoader.item
        }
    }

    QnLiteClientController
    {
        id: liteClientController
        serverId: clientId
    }

    Column
    {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -bottomLoader.height / 2
        width: liteClientControlScreen.availableWidth
        topPadding: screenDecoration.extraTopSize
        leftPadding: screenDecoration.extraLeftSize
        rightPadding: screenDecoration.extraRightSize

        readonly property real availableWidth: width - leftPadding - rightPadding

        Loader
        {
            id: screenLoader

            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(parent.availableWidth, 400)
            height: Math.floor(width / 16 * 9)

            ScreenDecoration
            {
                id: screenDecoration
                anchors.fill: parent
                z: -1
            }
        }

        Item
        {
            width: 1
            height: screenDecoration.extraBottomSize
        }

        Text
        {
            id: cameraName

            width: parent.width
            height: 40
            font.pixelSize: 16
            color: ColorTheme.windowText
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter

            text: d.screenItem ? d.screenItem.activeItem.resourceName : ""
        }
    }

    Loader
    {
        id: bottomLoader
        anchors.bottom: parent.bottom
        width: parent.width
        height: 168
    }

    Component
    {
        id: selectorComponent

        Selector
        {
            id: selector

            onCurrentResourceIdChanged:
            {
                if (d.screenItem)
                    d.screenItem.activeItem.resourceId = currentResourceId
            }

            Connections
            {
                target: d.screenItem
                onActiveItemChanged: selector.updateResourceId()
            }
            Connections
            {
                target: d
                onScreenItemChanged: selector.updateResourceId()
            }

            onFourCamerasModeChanged:
            {
                liteClientControlScreen.state =
                    (fourCamerasMode ? "MultipleCameras" : "SingleCamera")
            }

            function updateResourceId()
            {
                if (!d.screenItem.activeItem)
                    return

                selector.currentResourceId = d.screenItem.activeItem.resourceId
            }
        }
    }

    Component
    {
        id: launchButtonComponent

        LaunchButton
        {

        }
    }

    state: "SwitchedOff"
    states: [
        State
        {
            name: "SwitchedOff"
            when: !liteClientController.clientOnline

            PropertyChanges
            {
                target: screenLoader
                source: "private/LiteClientControlScreen/SwitchedOffDummy.qml"
            }
            PropertyChanges
            {
                target: bottomLoader
                sourceComponent: launchButtonComponent
            }
            PropertyChanges
            {
                target: enabledSwitch
                checked: false
            }
        },
        /*
        State
        {
            name: "NoDisplayConnected"
            PropertyChanges
            {
                target: screenLoader
                source: "private/LiteClientControlScreen/NoDisplayDummy.qml"
            }
            PropertyChanges
            {
                target: bottomLoader
                sourceComponent: undefined
            }
        },
        */
        State
        {
            name: "SingleCamera"
            when: liteClientController.clientOnline
                && liteClientController.displayMode == QnLiteClientController.SingleCamera

            PropertyChanges
            {
                target: screenLoader
                source: "private/LiteClientControlScreen/SingleCameraLayout.qml"
            }
            PropertyChanges
            {
                target: bottomLoader
                sourceComponent: selectorComponent
            }
            PropertyChanges
            {
                target: enabledSwitch
                checked: true
            }
        },
        State
        {
            name: "MultipleCameras"
            when: liteClientController.clientOnline
                && liteClientController.displayMode == QnLiteClientController.MultipleCameras

            PropertyChanges
            {
                target: screenLoader
                source: "private/LiteClientControlScreen/MultipleCamerasLayout.qml"
            }
            PropertyChanges
            {
                target: bottomLoader
                sourceComponent: selectorComponent
            }
            PropertyChanges
            {
                target: enabledSwitch
                checked: true
            }
        }
    ]
}
