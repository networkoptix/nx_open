import QtQuick 2.0

import "../base" as Base;
import "../../common" as Common;

Column
{
    id: thisComponent;

    signal selectAllClicked;

    spacing: Common.SizeManager.spacing.medium;

    anchors
    {
        left: parent.left;
        right: parent.right;

        leftMargin: Common.SizeManager.spacing.medium;
        rightMargin: Common.SizeManager.spacing.medium;
    }

    Base.EmptyCell {}

    Base.LineDelimiter
    {
        color: "#e4e4e4";
    }

    Base.StyledButton
    {
        id: selectAllButton;

        height: Common.SizeManager.clickableSizes.base;
        anchors
        {
            left: parent.left;
            right: parent.right;
        }

        text: qsTr("select all systems");

        onClicked: selectAllClicked();
    }

    Base.EmptyCell {}
}

