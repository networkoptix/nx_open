import QtQuick 2.0;

import "../base" as Base;
import "../../common" as Common;

Item
{
    id: thisComponent;

    property int componentFlags: 0;
    property int flags: 0;

    property Component item;

    height:1;
    width: 1;



    Component
    {
        Item
        {
            height: Common.SizeManager.clickableSizes.large * 2;
            anchors
            {
                left: (parent ? parent.left : undefined);
                right: (parent ? parent.right : undefined);
            }

            Base.Text
            {
                anchors.fill: parent;

                verticalAlignment: Text.AlignVCenter;
                horizontalAlignment: Text.AlignHCenter;

                text: "Can not change this parameter for selected server";
                wrapMode: Text.Wrap;
            }
        }
    }
}
