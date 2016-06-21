import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0

Item
{
    implicitHeight: column.implicitHeight

    Column
    {
        id: column

        width: parent.width
        topPadding: 24

        CloudBanner
        {
            id: cloudBanner
            text: cloudStatusWatcher.cloudLogin
        }

        Item
        {
            height: logoutButton.height + 40
            width: parent.width

            Button
            {
                id: logoutButton
                y: 24
                text: qsTr("Logout")
                width: parent.width

                onClicked: setCloudCredentials("", "")
            }
        }

        LinkButton
        {
            text: qsTr("Go to %1").arg(applicationInfo.cloudName())
            width: parent.width
        }
    }
}
