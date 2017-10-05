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
    property alias factorySystem: informationBlock.factorySystem
    property alias running: informationBlock.online
    property alias ownerDescription: informationBlock.ownerDescription
    property bool reachable: false
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
        localSystemId: control.localId
    }
    ModelDataAccessor
    {
        id: hostsModelAccessor
        model: hostsModel

        property string defaultAddress: ""

        onDataChanged:
        {
            if (startRow === 0)
                updateDefaultAddress()
        }

        onRowsMoved: updateDefaultAddress()
        onCountChanged: updateDefaultAddress()

        function updateDefaultAddress()
        {
            defaultAddress = count > 0
                ? getData(0, "url")
                : ""
        }
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
        color:
        {
            if (control.factorySystem)
                return ColorTheme.base10

            return control.enabled ? ColorTheme.base7 : ColorTheme.base6
        }

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
        address: Nx.url(hostsModelAccessor.defaultAddress).address()
        user: authenticationDataModel.defaultCredentials.user
        factoryDetailsText: d.kFactorySystemDetailsText
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

        visible: text !== ""

        anchors
        {
            bottom: parent.bottom
            bottomMargin: 12
            right: parent.right
            rightMargin: 12
        }

        color:
        {
            if (!compatible)
                return invalidVersion ? ColorTheme.yellow_main : ColorTheme.red_main

            return ColorTheme.base14
        }

        textColor: compatible ? ColorTheme.contrast8 : ColorTheme.base4

        text:
        {
            if (!compatible)
                return invalidVersion ? invalidVersion : qsTr("INCOMPATIBLE");

            if (!running)
                return qsTr("OFFLINE")

            if (!reachable)
                return qsTr("UNREACHABLE")

            return ""
        }
    }

    function open()
    {
        if (factorySystem)
        {
            Workflow.openStandardDialog("", d.kFactorySystemDetailsText)
            return
        }

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
                    hostsModelAccessor.defaultAddress,
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
                    hostsModelAccessor.defaultAddress,
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

    QtObject
    {
        id: d

        readonly property string kFactorySystemDetailsText:
            qsTr("Connect to this server from web browser or through desktop client to set it up")
    }

    Keys.onEnterPressed: open()
    Keys.onReturnPressed: open()
}
