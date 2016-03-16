import QtQuick 2.5;
import QtQuick.Controls 1.2;

Column
{
    id: thisComponent;

    property string userName;
    property bool loggedIn: false;

    signal loginToCloud();
    signal createAccount();

    signal manageAccount();
    signal logout();

    spacing: 10;

    Rectangle // TODO: change to image
    {

        width: 50;
        height: 50;
        anchors.horizontalCenter: parent.horizontalCenter;

        color: (loggedIn ? "green" : "red");
    }

    Text
    {
        id: userNameText;

        anchors.horizontalCenter: parent.horizontalCenter;
        text: (loggedIn ? thisComponent.userName
            :  qsTr("You are not logged to the cloud"));
        color: "yellow";    // todo: replace
    }

    Loader
    {
        id: buttonsLoader;

        anchors.horizontalCenter: parent.horizontalCenter;
        sourceComponent: (loggedIn ? loggedInButtons : disconnectedButtons);
    }

    //

    Component
    {
        id: loggedInButtons;

        Row
        {
            spacing: 10;

            Text
            {
                id: manageAccountLink;

                text: makeLink(qsTr("Manage account"));
                onLinkActivated: { thisComponent.manageAccount(); }
            }

            Text
            {
                id: logoutLink;

                text: makeLink(qsTr("Logout"));
                onLinkActivated: { thisComponent.logout(); }
            }
        }
    }

    Component
    {
        id: disconnectedButtons;

        Row
        {
            Button
            {
                id: loginButton;

                text: qsTr("Login");
                onClicked: { thisComponent.loginToCloud(); }
            }

            Button
            {
                id: createAccountButton;

                text: qsTr("Create account");
                onClicked: { thisComponent.createAccount(); }
            }
        }
    }

    function makeLink(caption, link)
    {
        return ("<a href=\"%2\">%1</a>").arg(caption)
            .arg(link ? link : "_some_placeholder_to_make_link_working");
    }
}
