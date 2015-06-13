import QtQuick 2.0

import "../base" as Base;
import "../../common" as Common;

Base.StatefulComponent
{
    id: thisComponent;

    property alias showItem: thisComponent.showFirst;
    property string message: qsTr("Can not change this parameter(s) for some selected servers");

    property alias item: thisComponent.first;

    second: Component
    {
        id: warningComponent;

        Item
        {
            id: warningPage;

            height: warningTextControl.height * 2;
            width: warningTextControl.width;

            Base.Text
            {
                id: warningTextControl;

                verticalAlignment: Text.AlignVCenter;
                horizontalAlignment: Text.AlignHCenter;
/*
                anchors
                {
                    left: parent.left;
                    leftMargin: Common.SizeManager.spacing.base;
                    verticalCenter: parent.verticalCenter;
                }
*/
                color: "red";
                text: thisComponent.message;
                wrapMode: Text.Wrap;

            }
        }
    }
}

