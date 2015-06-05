import QtQuick 2.4;
import QtQuick.Controls 1.3;
import QtQuick.Controls.Styles 1.3;

import "." as Rtu;
import ".." as Common;

Button
{
    id: thisComponent;

    implicitHeight: Common.SizeManager.clickableSizes.medium;
   
    property int fontSize: Common.SizeManager.fontSizes.medium;

    style: ButtonStyle
    {
        label: Rtu.Text
        {
            id: styleLabel;

            text: thisComponent.text;

            wrapMode: Text.Wrap;
            verticalAlignment: Text.AlignVCenter;
            horizontalAlignment: Text.AlignHCenter;
            font.pointSize: thisComponent.fontSize;
            
            anchors.fill: parent;
        }
    }
}

