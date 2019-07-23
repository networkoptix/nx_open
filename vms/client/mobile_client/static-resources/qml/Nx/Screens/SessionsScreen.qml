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

    leftButtonIcon.source: lp("/images/menu.png")
    padding: 16

    onLeftButtonClicked: sideNavigation.open()

    titleControls:
    [
        Button
        {
            text: qsTr("Log in to %1", "%1 is the short cloud name (like 'Cloud')").arg(applicationInfo.cloudName())
            textColor: ColorTheme.base16
            flat: true
            leftPadding: 0
            rightPadding: 0
            labelPadding: 8
            visible: cloudStatusWatcher.status == QnCloudStatusWatcher.LoggedOut
            onClicked: Workflow.openCloudScreen()
        }
    ]

    readonly property real margins: -4

    MouseArea
    {
        id: searchCanceller

        z: 1
        anchors.fill: parent

        onPressed:
        {
            mouse.accepted = false
            searchEdit.focus = false
        }
    }

    Item
    {
        id: searchPanel

        readonly property bool searching:
            visible && (searchEdit.activeFocus || searchEdit.text.length)

        readonly property point searchPanelPosition:
        {
            if (searchPanel.searching)
                return Qt.point(sessionsScreen.margins, sessionsScreen.margins)

            return sessionsList.mapToItem(
                searchPanel.parent,
                sessionsList.originX - sessionsList.contentX,
                sessionsList.originY - sessionsList.contentY)
        }

        x: searchPanelPosition.x
        y: searchPanelPosition.y
        z: 2

        height: searchEdit.height + 8
        width: sessionsList.width
        visible: sessionsList.model.sourceRowsCount > 8

        SearchEdit
        {
            id: searchEdit

            property Item placeholder: null

            height: 40
            width: parent.width
            onTextChanged: sessionsList.model.filterWildcard = text
        }
    }

    GridView
    {
        id: sessionsList

        clip: true
        x: sessionsScreen.margins
        y: sessionsScreen.margins + (searchPanel.searching ? searchPanel.height : 0)
        width: parent.width - sessionsScreen.margins * 2
        height: parent.height - sessionsScreen.margins * 2

        header: Item
        {
            width: sessionsList.width
            height: (searchPanel.searching || !searchPanel.visible) ? 0 : searchPanel.height
            onHeightChanged:
            {
                // Since we change header height in some cases it is hidden when list is scrolled to
                // the frist tile. This code fixes behaviour and shows search edit on top if tiles.
                if (sessionsList.contentY + sessionsList.originY== -height)
                    sessionsList.positionViewAtBeginning()
            }

            Component.onCompleted: searchEdit.placeholder = this
        }

        property real horizontalSpacing: 8
        property real verticalSpacing: cellsInRow > 1 ? 8 : 1
        readonly property real maxCellWidth: 488 + horizontalSpacing
        property int cellsInRow: Math.max(1, Math.ceil(width / maxCellWidth))

        cellWidth: (width - leftMargin - rightMargin) / cellsInRow
        cellHeight: 98 + verticalSpacing

        model: OrderedSystemsModel
        {
            id: systemsModel
            minimalVersion: "2.5"
            filterCaseSensitivity: Qt.CaseInsensitive;
            filterRole: 257; //
        }

        delegate: Item
        {
            width: sessionsList.cellWidth
            height: sessionsList.cellHeight

            SessionItem
            {
                anchors.fill: parent
                anchors.leftMargin: sessionsList.horizontalSpacing / 2
                anchors.rightMargin: sessionsList.horizontalSpacing / 2
                anchors.topMargin: Math.floor(sessionsList.verticalSpacing / 2)
                anchors.bottomMargin: sessionsList.verticalSpacing - anchors.topMargin

                systemName: model.systemName
                systemId: model.systemId
                localId: model.localId
                cloudSystem: model.isCloudSystem
                factorySystem: model.isFactorySystem
                ownerDescription: cloudSystem ? model.ownerDescription : ""
                running: model.isRunning
                reachable: model.isReachable
                compatible: model.isCompatibleToMobileClient || model.isFactorySystem

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

        readonly property string kCheckNetworkMessage:
            qsTr("Check your network connection or press \"%1\" button to enter a known server address.",
             "%1 is a button name").arg(customConnectionButton.text)

        anchors.fill: parent
        anchors.topMargin: -16
        anchors.bottomMargin: -24

        readonly property bool emptySearchMode:
            searchPanel.searching && sessionsList.model.sourceRowsCount > 0

        title: emptySearchMode ? qsTr("Nothing found") : qsTr("No Systems found")
        description: emptySearchMode ? "" : kCheckNetworkMessage

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
                    qsTr("Cannot connect to System \"%1\"", "%1 is a system name").arg(systemName) :
                    qsTr("Cannot connect to Server")
        Workflow.openStandardDialog(
            message, qsTr("Check your network connection or contact a system administrator"))
    }
}
