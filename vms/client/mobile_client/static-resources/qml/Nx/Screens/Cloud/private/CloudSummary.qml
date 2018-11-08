import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0

Item
{
    implicitHeight: column.implicitHeight

    signal loggedOut()

    Column
    {
        id: column

        width: parent.width
        topPadding: 24

        CloudBanner
        {
            id: cloudBanner
            text: cloudStatusWatcher.effectiveUserName
        }

        Item { width: 1; height: 18 }

        Button
        {
            id: logoutButton
            text: qsTr("Log out")
            width: parent.width

            onClicked:
            {
                setCloudCredentials("", "")
                loggedOut()
            }
        }

        Item { width: 1; height: 10 }

        LinkButton
        {
            text: qsTr("Go to %1").arg(applicationInfo.cloudName())
            width: parent.width
            onClicked: Qt.openUrlExternally(cloudUrlHelper.mainUrl())
        }
    }
}
