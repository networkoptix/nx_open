import QtQuick 2.5;
import QtQuick.Controls 1.2;


Column
{
    property string userName;
    property bool loggedIn: (userName.length !== 0);

    signal loginToCloud();
    signal createAccount();

    signal manageAccount();
    signal logout();

    Rectangle // TODO: change to image
    {

    }

    Component
    {
        id: loggedInButtons;

        Row
        {

        }
    }

    Component
    {
        id: disconnectedButtons;
    }

    Loader
    {
        id: buttonsLoader;

        sourceComponent: (loggedIn ? loggedInButtons : disconnectedButtons);
    }
}
