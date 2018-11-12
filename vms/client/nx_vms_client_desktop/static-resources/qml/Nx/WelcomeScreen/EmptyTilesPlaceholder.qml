import QtQuick 2.6;
import Nx 1.0;

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
        font.pixelSize: 24;
        font.weight: Font.Light;
        color: ColorTheme.windowText;
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
        font.pixelSize: 13;
        font.weight: Font.Light;
        color: ColorTheme.windowText;
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
            qsTr("Check your network connection or press \"Connect to Server\" button to enter known server address");
    }
}
