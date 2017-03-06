import QtQuick 2.6
import QtQuick.Layouts 1.1
import Qt.labs.controls 1.0
import Nx 1.0
import Nx.Controls 1.0
import Nx.Models 1.0
import com.networkoptix.qml 1.0

Pane
{
    id: control

    property alias systemId: informationBlock.systemId
    property alias localId: informationBlock.localId
    property alias systemName: informationBlock.systemName
    property alias cloudSystem: informationBlock.cloud
    property alias online: informationBlock.online
    property alias ownerDescription: informationBlock.ownerDescription
    property bool compatible: true
    property string invalidVersion

    readonly property string kMinimimVersion: "1.3.1"

    padding: 0

    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
    implicitWidth: 200

    SystemHostsModel
    {
        id: hostsModel
        systemId: control.systemId
    }
    AuthenticationDataModel
    {
        id: authenticationDataModel
        systemId: control.localId

        readonly property bool hasData: !!defaultCredentials.user
        readonly property bool hasStoredPassword: !!defaultCredentials.password
    }

    background: Rectangle
    {
        color: control.enabled ? ColorTheme.base7 : ColorTheme.base6
        radius: 2

        MaterialEffect
        {
            anchors.fill: parent
            mouseArea: mouseArea
            clip: true
            rippleSize: width * 0.8
        }
    }

    MouseArea
    {
        id: mouseArea

        parent: control
        anchors.fill: parent
        onClicked: open()
    }

    contentItem: SystemInformationBlock
    {
        id: informationBlock
        enabled: compatible && online
        address: Nx.url(hostsModel.firstHost).address()
        user: authenticationDataModel.defaultCredentials.user
    }

    IconButton
    {
        /* It should not be parented by contentItem */
        parent: control

        y: 2
        z: 1
        anchors.right: parent.right
        icon: lp("/images/edit.png")
        visible: !cloudSystem && authenticationDataModel.hasData
        onClicked:
        {
            Workflow.openSavedSession(
                systemId,
                localId,
                systemName,
                informationBlock.address,
                informationBlock.user,
                authenticationDataModel.defaultCredentials.password)
        }
    }

    IssueLabel
    {
        id: issueLabel
        anchors
        {
            bottom: parent.bottom
            bottomMargin: 12
            right: parent.right
            rightMargin: 12
        }
        color: cloudSystem ? ColorTheme.base14 : ColorTheme.red_main
        visible: text !== ""
        text:
        {
            if (cloudSystem)
            {
                return !online && cloudStatusWatcher.status === QnCloudStatusWatcher.Online
                    ? qsTr("OFFLINE") : ""
            }

            return compatible ? "" : (invalidVersion || qsTr("INCOMPATIBLE"))
        }
    }

    function open()
    {
        if (!compatible)
        {
            if (Nx.softwareVersion(invalidVersion).isLessThan(Nx.softwareVersion(kMinimimVersion))
                || applicationInfo.oldMobileClientUrl() == "")
            {
                Workflow.openStandardDialog("",
                    qsTr("This server has too old version. "
                        + "Please update it to the latest version."))
                return
            }

            Workflow.openOldClientDownloadSuggestion()
            return
        }

        if (cloudSystem)
        {
            if (!hostsModel.isEmpty)
            {
                if (!connectionManager.connectToServer(
                    hostsModel.firstHost,
                    cloudStatusWatcher.credentials.user,
                    cloudStatusWatcher.credentials.password))
                {
                    sessionsScreen.openConnectionWarningDialog(systemName)
                    return
                }

                Workflow.openResourcesScreen(systemName)
            }
        }
        else
        {
            if (authenticationDataModel.hasStoredPassword)
            {
                if (!connectionManager.connectToServer(
                    hostsModel.firstHost,
                    authenticationDataModel.defaultCredentials.user,
                    authenticationDataModel.defaultCredentials.password))
                {
                    sessionsScreen.openConnectionWarningDialog(systemName)
                    return
                }

                Workflow.openResourcesScreen(systemName)
            }
            else
            {
                Workflow.openDiscoveredSession(
                    systemId, localId, systemName, informationBlock.address)
            }
        }
    }

    Keys.onEnterPressed: open()
    Keys.onReturnPressed: open()
}
