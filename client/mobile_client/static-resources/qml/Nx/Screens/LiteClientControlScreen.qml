import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0
import Nx.Core 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0

import "private/LiteClientControlScreen"

Page
{
    id: liteClientControlScreen
    objectName: "liteClientControlScreen"

    property string clientId

    leftPadding: 8
    rightPadding: 8

    screenOrientation: (d.minimumHeight > width || d.minimumHeight > height)
        ? Qt.PortraitOrientation : Qt.PrimaryOrientation

    leftButtonIcon: lp("/images/menu.png")
    onLeftButtonClicked: sideNavigation.open()

    ResourceHelper
    {
        id: resourceHelper
        resourceId: clientId
    }


    title: "%1 (%2)".arg(applicationInfo.liteDeviceName()).arg(resourceHelper.resourceName)

    titleControls:
    [
        Switch
        {
            id: enabledSwitch

            onClicked:
            {
                if (checked)
                    liteClientController.startLiteClient()
                else
                    liteClientController.stopLiteClient()
            }
        }
    ]

    QtObject
    {
        id: d

        readonly property real minimumHeight: 316 + bottomLoader.height + header.height

        property int activeItemIndex: 0

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

    MediaResourceHelper
    {
        id: currentResourceHelper
        resourceId: d.currentResourceId
    }

    QnLiteClientController
    {
        id: liteClientController
        serverId: clientId

        onClientStartError:
        {
            Workflow.openStandardDialog(
                qsTr("Cannot start client"),
                qsTr("Please make sure that display is connected to %1.")
                    .arg(applicationInfo.liteDeviceName()))
        }

        onClientStopError:
        {
            Workflow.openStandardDialog(qsTr("Cannot stop client"))
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

        Text
        {
            width: parent.width
            font.pixelSize: 13
            color: ColorTheme.red_main
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap

            text: qsTr("Software image rotation is not supported in fullscreen mode")

            visible: liteClientControlScreen.state === "SingleCamera"
                && d.currentResourceHelper !== ""
                && (currentResourceHelper.customRotation !== 0
                    || currentResourceHelper.customAspectRatio !== 0.0)
        }
    }

    Loader
    {
        id: bottomLoader
        anchors.bottom: parent.bottom
        width: parent.width
        height: 168
    }

    DummyMessage
    {
        id: offlineDummy
        anchors.fill: parent
        title: qsTr("%1 is offline").arg(applicationInfo.liteDeviceName())
        image: lp("/images/alert_nx1_offline.png")
        visible: !liteClientController.serverOnline
    }

    Component
    {
        id: selectorComponent

        Selector
        {
            id: selector

            currentResourceId: d.currentResourceId
            fourCamerasMode: (liteClientController.layoutHelper.displayMode
               === LiteClientLayoutHelper.MultipleCameras)

            onSingleCameraModeClicked:
            {
                liteClientController.layoutHelper.displayCell =
                    Qt.point(d.screenItem.activeItem.layoutX, d.screenItem.activeItem.layoutY)
            }

            onMultipleCmaerasModeClicked:
            {
                liteClientController.layoutHelper.displayCell = Qt.point(-1, -1)
            }

            onCameraClicked:
            {
                if (!d.screenItem || !d.screenItem.activeItem)
                    return

                if (fourCamerasMode)
                {
                    liteClientController.layoutHelper.setCameraIdOnCell(
                        d.screenItem.activeItem.layoutX,
                        d.screenItem.activeItem.layoutY,
                        resourceId)
                }
                else
                {
                    liteClientController.layoutHelper.singleCameraId = resourceId
                }
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
            activeItem.layoutHelper: liteClientController.layoutHelper
        }
    }

    Component
    {
        id: multipleCamerasLayoutComponent

        MultipleCamerasLayout
        {
            layoutHelper: liteClientController.layoutHelper
            Component.onCompleted: activeItemIndex = d.activeItemIndex
            onActiveItemIndexChanged: d.activeItemIndex = activeItemIndex
        }
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
                sourceComponent: undefined
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
                && liteClientController.clientState === QnLiteClientController.Started
                && liteClientController.layoutHelper.displayMode
                    === LiteClientLayoutHelper.SingleCamera

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
                && liteClientController.clientState === QnLiteClientController.Started
                && liteClientController.layoutHelper.displayMode
                    === LiteClientLayoutHelper.MultipleCameras

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
