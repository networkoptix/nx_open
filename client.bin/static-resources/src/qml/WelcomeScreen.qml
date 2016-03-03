import QtQuick 2.1;

Item
{
    anchors.fill: parent;

    Grid
    {
        anchors.centerIn: parent;

        Repeater
        {
            model: context.createSystemsModel();

            delegate: Rectangle
            {
                width: 200;
                height: 100;

                color: "green";

                Text
                {
                    anchors.centerIn: parent;
                    text: model.systemName;
                }
            }
        }
    }
}
