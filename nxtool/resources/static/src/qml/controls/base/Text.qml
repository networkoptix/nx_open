import QtQuick 2.1 as QtQuick;

import "../../common" as Common;

QtQuick.Text
{
    id: thisComponent;

    property bool thin: true;    
   
    font.family: (thin ? Common.SizeManager.regularFont.name
        : Common.SizeManager.mediumFont.name );

    font.pixelSize: Common.SizeManager.fontSizes.base;
}

