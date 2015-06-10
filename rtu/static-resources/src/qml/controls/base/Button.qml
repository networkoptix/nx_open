import QtQuick 2.1;
import QtQuick.Controls 1.1 as QtControls;
import QtQuick.Controls.Styles 1.1;

import "." as Base;
import "../../common" as Common;

QtControls.Button
{
    id: thisComponent;

    implicitHeight: Common.SizeManager.clickableSizes.medium;
   
    property int fontSize: Common.SizeManager.fontSizes.medium;

    style: ButtonStyle
    {
        label: Base.Text
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

