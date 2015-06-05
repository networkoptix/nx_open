pragma Singleton

import QtQuick 2.3;
import QtQuick.Controls 1.2;

QtObject
{
    id: thisComponent;
    
    property QtObject clickableSizes: QtObject
    {
        readonly property real base: impl.sizeSourceButton.height;
        readonly property real medium: base * 1.5;
        readonly property real large: medium * 1.5;
    }

    property QtObject fontSizes: QtObject
    {
        readonly property real small: impl.text.font.pointSize * 1.1;
        readonly property real base: small * 1.1;
        readonly property real medium: base * 1.1;
        readonly property real large: medium * 1.1;
    }

    property QtObject spacing: QtObject
    {
        readonly property real base: clickableSizes.base / 2;
        readonly property real medium: clickableSizes.base / 1.5;
        readonly property real small: clickableSizes.base / 5;
        readonly property real large: clickableSizes.base;
        readonly property real extraLarge: clickableSizes.base * 2;
    }
    
    property QtObject lineSizes: QtObject
    {
        readonly property real thin: 1;
        readonly property real thick: thin * 2;
    }

    property QtObject impl: QtObject
    {
        readonly property int kInvalidArrayIndex: -1;
        
        property Button sizeSourceButton: Button { text: "0"; }
        
        property Text text: Text {}
    }
    
    Component.onCompleted: 
    {
        console.log(">>> Size manager custom properties:");
        console.log();
        
        var baseProperties = ["impl", "implChanged", "objectName", "objectNameChanged"];
        var propertyChangesCont = [];
        
        for (var propertyName in thisComponent)
        {
            
            if ((baseProperties.indexOf(propertyName) === impl.kInvalidArrayIndex ) 
                && !propertyChangesCont[propertyName])
            {
                propertyChangesCont[propertyName] = true;
                propertyChangesCont[propertyName + "Changed"] = true;
                
                console.log(propertyName + ": " + thisComponent[propertyName]);
            }
        }

        console.log();
        console.log("<<<");
    }
}

