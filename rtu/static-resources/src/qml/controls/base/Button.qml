import QtQuick 2.1;
import QtQuick.Controls 1.1 as QtControls;
import QtQuick.Controls.Styles 1.1;

import "." as Base;
import "../../common" as Common;

QtControls.Button
{
    id: thisComponent;

    property int fontSize: Common.SizeManager.fontSizes.medium;
    
    implicitHeight: Common.SizeManager.clickableSizes.medium;
    opacity: (enabled ? 1.0 : 0.5);
   
    activeFocusOnPress: true;
    
    style: ButtonStyle
    {
        label: Base.Text
        {
            id: styleLabel;

            text: thisComponent.text;

            wrapMode: Text.Wrap;
            verticalAlignment: Text.AlignVCenter;
            horizontalAlignment: Text.AlignHCenter;
            font.pixelSize: thisComponent.fontSize;
            
            visible: thisComponent.visible;
            anchors.fill: parent;
        }
    }
}

