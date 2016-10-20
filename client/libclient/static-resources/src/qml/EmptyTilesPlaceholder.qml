import QtQuick 2.6;

import "."

Column
{
    property int foundServersCount: 0;

    spacing: 24;

    NxLabel
    {
        id: caption;

        width: 560;
        anchors.horizontalCenter: parent.horizontalCenter;
        horizontalAlignment: Text.AlignHCenter;
        wrapMode: Text.WordWrap;

        text: (searchingTimer.running ? impl.kSearchingServersText : impl.kNoServersFoundText);
        font: Style.fonts.notFoundMessages.caption;
        color: Style.colors.windowText;
    }

    NxLabel
    {
        id: description;

        width: 360;
        anchors.horizontalCenter: parent.horizontalCenter;
        horizontalAlignment: Text.AlignHCenter;
        visible: !searchingTimer.running;
        wrapMode: Text.WordWrap;

        text: impl.kDescription;
        font: Style.fonts.notFoundMessages.description;
        color: Style.colors.windowText;
    }

    Timer
    {
        id: searchingTimer;

        interval: 10 * 1000;
    }

    Component.onCompleted:
    {
        if (!foundServersCount)
            searchingTimer.start();
    }

    onFoundServersCountChanged:
    {
        if (foundServersCount)
            searchingTimer.stop();
    }

    property QtObject impl: QtObject
    {
        readonly property string kSearchingServersText:
            qsTr("Searching servers in your local network...");
        readonly property string kNoServersFoundText:
            qsTr("No servers found");
        readonly property string kDescription:
            qsTr("Check your network connection or press \"Connect to System\" button to enter known server address");
    }
}
