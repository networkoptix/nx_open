import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import Nx.Models 1.0
import com.networkoptix.qml 1.0

Page
{
    id: sessionsScreen
    objectName: "sessionsScreen"

    leftButtonIcon: lp("/images/menu.png")
    padding: 16

    onLeftButtonClicked: sideNavigation.open()

    titleControls:
    [
        Button
        {
            text: qsTr("Log in to %1").arg(applicationInfo.cloudName())
            textColor: ColorTheme.base16
            flat: true
            leftPadding: 0
            rightPadding: 0
            labelPadding: 8
            visible: cloudStatusWatcher.status == QnCloudStatusWatcher.LoggedOut
            onClicked: Workflow.openCloudScreen()
        }
    ]

    Object
    {
        id: d

        property bool cloudOffline: cloudStatusWatcher.status == QnCloudStatusWatcher.Offline

        onCloudOfflineChanged:
        {
            if (!cloudOffline)
                warningVisible = false
        }

        Timer
        {
            id: cloudWarningDelay
            interval: 5000
            running: d.cloudOffline
            onTriggered:
            {
                // Don't overwrite the current warning
                if (warningVisible)
                    return

                warningText = qsTr("Cannot connect to %1").arg(applicationInfo.cloudName())
                warningVisible = true
            }
        }
    }

    GridView
    {
        id: sessionsList

        anchors.fill: parent
        anchors.margins: -4

        property real spacing: 8
        readonly property real minCellWidth: 300 + spacing
        property int cellsInRow: Math.max(1, Math.floor(width / minCellWidth))

        cellWidth: (width - leftMargin - rightMargin) / cellsInRow
        cellHeight: 98 + spacing

        model: OrderedSystemsModel
        {
            id: systemsModel
            minimalVersion: "2.5"
        }

        delegate: Item
        {
            width: sessionsList.cellWidth
            height: sessionsList.cellHeight

            SessionItem
            {
                anchors.fill: parent
                anchors.margins: sessionsList.spacing / 2

                systemName: model.systemName
                systemId: model.systemId
                localId: model.localId
                cloudSystem: model.isCloudSystem
                ownerDescription: cloudSystem ? model.ownerDescription : ""
                online: model.isConnectable
                compatible: model.isCompatible
                invalidVersion: model.wrongVersion ? model.wrongVersion.toString() : ""
            }
        }
        highlight: Rectangle
        {
            width: sessionsList.cellWidth
            height: sessionsList.cellHeight
            z: 2.0
            color: "transparent"
            border.color: ColorTheme.contrast9
            border.width: 4
            visible: liteMode
        }
        highlightMoveDuration: 0

        displayMarginBeginning: 16
        displayMarginEnd: 16 + mainWindow.bottomPadding

        focus: liteMode

        Keys.onPressed:
        {
            // TODO: #dklychkov Implement transparent navigation to the footer item.
            if (event.key == Qt.Key_C)
            {
                Workflow.openNewSessionScreen()
                event.accepted = true
            }
        }
    }

    footer: Rectangle
    {
        implicitHeight: customConnectionButton.implicitHeight + 16
        color: ColorTheme.windowBackground

        Rectangle
        {
            width: parent.width
            height: 1
            anchors.top: parent.top
            color: ColorTheme.base4
        }

        Button
        {
            id: customConnectionButton

            text: dummyMessage.visible
                ? qsTr("Connect to Server...")
                : qsTr("Connect to Another Server...")

            anchors.centerIn: parent
            width: parent.width - 16

            onClicked: Workflow.openNewSessionScreen()
        }
    }

    DummyMessage
    {
        id: dummyMessage

        anchors.fill: parent
        anchors.topMargin: -16
        anchors.bottomMargin: -24
        title: qsTr("No Systems found")
        description: qsTr(
             "Check your network connection or press \"%1\" button "
                 + "to enter a known server address.").arg(customConnectionButton.text)
        visible: false

        Timer
        {
            interval: 1000
            running: true
            onTriggered:
            {
                dummyMessage.visible = Qt.binding(function() { return sessionsList.count == 0 })
            }
        }
    }

    function openConnectionWarningDialog(systemName)
    {
        var message = systemName ?
                    qsTr("Cannot connect to System \"%1\"").arg(systemName) :
                    qsTr("Cannot connect to Server")
        Workflow.openStandardDialog(
            message, qsTr("Check your network connection or contact a system administrator"))
    }
}
