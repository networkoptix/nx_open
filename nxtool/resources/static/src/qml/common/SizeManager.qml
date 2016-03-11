pragma Singleton

import QtQuick 2.1;
import QtQuick.Controls 1.1;


QtObject
{
    id: thisComponent;
    
    property alias regularFont: regularCustomFont;
    property alias mediumFont: mediumCustomFont;

    property QtObject clickableSizes: QtObject
    {
        readonly property real small: base * 0.5; 
        readonly property real base: impl.sizeSourceButton.height;
        readonly property real medium: base * 1.3;
        readonly property real large: medium * 1.3;
    }
    
    property QtObject fontSizes: QtObject
    {
        readonly property real small: 12;
        readonly property real base: 14;
        readonly property real medium: 16;
        readonly property real large: 19;
    }

    property QtObject spacing: QtObject
    {
        readonly property real small: clickableSizes.base / 8;
        readonly property real base: clickableSizes.base / 3;
        readonly property real medium: clickableSizes.base / 1.5;
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
        
        property var font: FontLoader
        {
            id: regularCustomFont;
            source: "qrc:/resources/Roboto-Regular.ttf";
        }
        property var mediumFont: FontLoader
        {
            id: mediumCustomFont;
            source: "qrc:/resources/Roboto-Medium.ttf";
        }
    }
}

