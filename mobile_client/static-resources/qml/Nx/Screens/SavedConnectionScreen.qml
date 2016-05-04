import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0

Page
{
    id: customConnectionScreen

    onLeftButtonClicked: Workflow.popCurrentScreen()

    property alias host: credentialsEditor.host
    property alias port: credentialsEditor.port
    property alias login: credentialsEditor.login
    property alias password: credentialsEditor.password
    property string sessionId

    Column
    {
        width: parent.width
        leftPadding: 16
        rightPadding: 16
        spacing: 24

        property real contentWidth: width - leftPadding - rightPadding

        SessionCredentialsEditor
        {
            id: credentialsEditor
            width: parent.contentWidth
        }

        Row
        {
            width: parent.contentWidth

            Button
            {
                width: parent.width * 0.6
                text: qsTr("Save")
                onClicked:
                {
                    savedSessionsModel.updateSession(sessionId, host, port, login, password, title, false)
                    Workflow.popCurrentScreen()
                }
            }

            Button
            {
                width: parent.width * 0.4
                text: qsTr("Delete")
                color: ColorTheme.orange_main

                onClicked:
                {
                    savedSessionsModel.deleteSession(sessionId)
                    Workflow.popCurrentScreen()
                }
            }
        }
    }
}
