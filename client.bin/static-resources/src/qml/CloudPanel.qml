import QtQuick 2.5;
import QtQuick.Controls 1.2;

import "."

Item
{
    id: thisComponent;

    property string userName;
    property bool loggedIn: false;

    signal loginToCloud();
    signal createAccount();

    signal manageAccount();
    signal logout();

    width: 576;
    height: 168;

    Image
    {
        id: statusImage;

        width: 64;
        height: 64;
        anchors.top: parent.top;
        anchors.horizontalCenter: parent.horizontalCenter;

        source: (thisComponent.loggedIn
            ? "qrc:/skin/welcome_page/cloud_account_logged.png"
            : "qrc:/skin/welcome_page/cloud_account_not-logged.png");
    }

    NxLabel
    {
        id: userNameText;

        height: 32;
        anchors.top: statusImage.bottom;
        anchors.topMargin: 8;
        anchors.horizontalCenter: parent.horizontalCenter;
        text: (loggedIn ? thisComponent.userName
            :  qsTr("You are not logged in to the cloud"));

        color: Style.colors.text;
        font: Style.fonts.banner.userName;
    }

    Loader
    {
        id: buttonsLoader;

        anchors.top: userNameText.bottom;
        anchors.topMargin: (thisComponent.loggedIn ? 22 : 16);
        anchors.horizontalCenter: parent.horizontalCenter;
        sourceComponent: (loggedIn ? loggedInButtons : disconnectedButtons);
    }

    Component
    {
        id: loggedInButtons;

        Row
        {
            spacing: 12;

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
            spacing: 8;

            NxButton
            {
                id: loginButton;

                isAccentButton: true;
                text: qsTr("Login");
                onClicked: { thisComponent.loginToCloud(); }
            }

            NxButton
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
