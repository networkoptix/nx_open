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
                if (checked)
                {
                    liteClientController.startLiteClient()
                }
                else
                {
                    liteClientController.stopLiteClient()
                }
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

        property string currentResourceId: (screenItem && screenItem.activeItem
            ? screenItem.activeItem.resourceId : "")
    }

    QnLiteClientController
    {
        id: liteClientController
        serverId: clientId

        onClientStartError:
        {
            Workflow.openInformationDialog(
                qsTr("Cannot start client"),
                qsTr("Please make sure that display is connected to Nx1."))
        }

        onClientStopError:
        {
            Workflow.openInformationDialog(qsTr("Cannot stop client"))
        }
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
                visible: liteClientController.serverOnline
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

            text: d.screenItem && d.screenItem.activeItem ? d.screenItem.activeItem.resourceName : ""
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
                if (!d.screenItem)
                    return

                if (fourCamerasMode)
                {
                    liteClientController.layoutHelper.setCameraIdOnCell(
                        d.screenItem.activeItem.layoutX,
                        d.screenItem.activeItem.layoutY,
                        currentResourceId)
                }
                else
                {
                    liteClientController.layoutHelper.singleCameraId = currentResourceId
                }
            }

            Binding
            {
                target: selector
                property: "currentResourceId"
                value: d.currentResourceId
            }

            Binding
            {
                target: selector
                property: "fourCamerasMode"
                value: liteClientController.layoutHelper.displayMode
            }

            onFourCamerasModeChanged:
            {
                liteClientController.layoutHelper.displayMode = (fourCamerasMode
                    ? QnLiteClientLayoutHelper.MultipleCameras
                    : QnLiteClientLayoutHelper.SingleCamera)
            }
        }
    }

    Component
    {
        id: launchButtonComponent

        LaunchButton
        {
            onButtonClicked: liteClientController.startLiteClient()
        }
    }

    Component
    {
        id: singleCameraLayoutComponent

        SingleCameraLayout
        {
            activeItem.resourceId: liteClientController.layoutHelper.singleCameraId
        }
    }

    Component
    {
        id: multipleCamerasLayoutComponent

        MultipleCamerasLayout
        {
            id: layout

            Connections
            {
                target: liteClientController.layoutHelper
                onCameraIdChanged:
                {
                    var item = layout.itemAt(x, y)
                    if (!item)
                        return

                    item.resourceId = resourceId
                }
                onLayoutChanged: layout.resetLayout()
            }

            Component.onCompleted: resetLayout()

            function resetLayout()
            {
                for (var y = 0; y < gridHeight; ++y)
                {
                    for (var x = 0; x < gridWidth; ++x)
                    {
                        var item = layout.itemAt(x, y)
                        item.resourceId = liteClientController.layoutHelper.cameraIdOnCell(x, y)
                    }
                }
            }
        }
    }

    Component
    {
        id: serverOfflineDummyComponent
        ServerOfflineDummy {}
    }

    Component
    {
        id: switchedOffDummyComponent
        SwitchedOffDummy {}
    }

    Component
    {
        id: noDisplayDummyComponent
        NoDisplayDummy {}
    }

    Component
    {
        id: startingDummyComponent
        StartingDummy {}
    }

    Component
    {
        id: stoppingDummyComponent
        StoppingDummy {}
    }

    state: "SwitchedOff"
    states: [
        State
        {
            name: "Offline"
            when: !liteClientController.serverOnline

            PropertyChanges
            {
                target: screenLoader
                sourceComponent: serverOfflineDummyComponent
            }
            PropertyChanges
            {
                target: bottomLoader
                sourceComponent: undefined
            }
            PropertyChanges
            {
                target: enabledSwitch
                checked: false
                enabled: false
            }
        },
        State
        {
            name: "SwitchedOff"
            when: liteClientController.serverOnline
                && liteClientController.clientState == QnLiteClientController.Stopped

            PropertyChanges
            {
                target: screenLoader
                sourceComponent: switchedOffDummyComponent
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
                enabled: true
            }
        },
        /*
        State
        {
            name: "NoDisplayConnected"
            PropertyChanges
            {
                target: screenLoader
                sourceComponent: noDisplayDummyComponent
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
            name: "Starting"
            when: liteClientController.serverOnline
                && liteClientController.clientState == QnLiteClientController.Starting

            PropertyChanges
            {
                target: screenLoader
                sourceComponent: startingDummyComponent
            }
            PropertyChanges
            {
                target: bottomLoader
                sourceComponent: undefined
            }
            PropertyChanges
            {
                target: enabledSwitch
                checked: true
                enabled: false
            }
        },
        State
        {
            name: "Stopping"
            when: liteClientController.serverOnline
                && liteClientController.clientState == QnLiteClientController.Stopping

            PropertyChanges
            {
                target: screenLoader
                sourceComponent: stoppingDummyComponent
            }
            PropertyChanges
            {
                target: bottomLoader
                sourceComponent: undefined
            }
            PropertyChanges
            {
                target: enabledSwitch
                checked: false
                enabled: false
            }
        },
        State
        {
            name: "SingleCamera"
            when: liteClientController.serverOnline
                && liteClientController.clientState == QnLiteClientController.Started
                && liteClientController.layoutHelper.displayMode == QnLiteClientLayoutHelper.SingleCamera

            PropertyChanges
            {
                target: screenLoader
                sourceComponent: singleCameraLayoutComponent
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
                enabled: true
            }
        },
        State
        {
            name: "MultipleCameras"
            when: liteClientController.serverOnline
                && liteClientController.clientState == QnLiteClientController.Started
                && liteClientController.layoutHelper.displayMode == QnLiteClientLayoutHelper.MultipleCameras

            PropertyChanges
            {
                target: screenLoader
                sourceComponent: multipleCamerasLayoutComponent
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
                enabled: true
            }
        }
    ]
}
