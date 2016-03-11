import QtQuick 2.1;
import QtQuick.Controls 1.1;

import "../base" as Base;
import "../../common" as Common;

Base.Column
{
    id: thisComponent;

    property alias caption: captionText.text;

    anchors
    {
        left: parent.left;
        right: parent.right;

        leftMargin: Common.SizeManager.spacing.medium;
        rightMargin: Common.SizeManager.spacing.medium;
    }

    Base.EmptyCell {}

    Base.Text
    {
        id: captionText;

        thin: false;
        width: parent.width;
        font.pixelSize: Common.SizeManager.fontSizes.medium;
    }

    Base.LineDelimiter {}

    Base.Text
    {
        id: description;
        color: "red";

        width: parent.width;
        wrapMode: Text.Wrap;
        text: qsTr("We can't connect to these servers or find any information about them");
    }

    Base.EmptyCell {}
}
