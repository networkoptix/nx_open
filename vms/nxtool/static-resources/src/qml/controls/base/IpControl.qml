import QtQuick 2.3;

import "." as Base;
import "../../common" as Common;

FocusScope
{
    id: thisComponent;

    property real fontSize: Common.SizeManager.fontSizes.base;
    property bool changed: (firstOctet.changed || secondOctet.changed
        || thirdOctet.changed || fourthOctet.changed);
    property string text: ("%1.%2.%3.%4").arg(firstOctet.text)
        .arg(secondOctet.text).arg(thirdOctet.text).arg(fourthOctet.text);
    property bool isEmptyAddress:
        (!firstOctet.text.trim().length
        && !secondOctet.text.trim().length
        && !thirdOctet.text.trim().length
        && !fourthOctet.text.trim().length)

    property bool acceptableInput: firstOctet.acceptableInput
        && secondOctet.acceptableInput && thirdOctet.acceptableInput
        && fourthOctet.acceptableInput;

    property string initialText;

    opacity: (enabled ? 1 : 0.5);
    onInitialTextChanged:
    {
        /// fills octets with initial values
        var arr = initialText.split(".");
        if (arr.length != 4)    /// should be 4 octets
            arr = ['', '', '', ''];

        firstOctet.initialText = arr[0];
        secondOctet.initialText = arr[1];
        thirdOctet.initialText = arr[2];
        fourthOctet.initialText = arr[3];
    }

    onActiveFocusChanged:
    {
        if (activeFocus)
        {
            if (!firstOctet.activeFocus && !secondOctet.activeFocus
                && !thirdOctet.activeFocus && !fourthOctet.activeFocus)
            {
                firstOctet.forceActiveFocus();
            }
        }
        else
        {
            firstOctet.focus = true;
        }
    }

    Rectangle
    {
        anchors.fill: parent
        anchors.bottomMargin: -1
        color: "#44ffffff"
        radius: baserect.radius
    }

    Rectangle
    {
        id: baserect
        gradient: Gradient {
            GradientStop {color: "#e0e0e0" ; position: 0}
            GradientStop {color: "#fff" ; position: 0.1}
            GradientStop {color: "#fff" ; position: 1}
        }
        radius: firstOctet.contentHeight * 0.16
        anchors.fill: parent
        border.color: thisComponent.activeFocus ? "#47b" : "#999"
    }

    width: row.width * 1.2;
    height: Common.SizeManager.clickableSizes.medium;

    MouseArea
    {
        anchors.fill: parent;
        acceptedButtons: Qt.NoButton; // pass all clicks to parent
        cursorShape: Qt.IBeamCursor;
    }

    Row
    {
        id: row;

        spacing: 3;
        anchors.centerIn: parent;

        IpOctet
        {
            id: firstOctet;
            nextOctet: secondOctet;
            //prevOctet: thisComponent.KeyNavigation.backtab;
        }

        IpDot
        {
            parentOctet: firstOctet;
        }

        IpOctet
        {
            id: secondOctet;
            nextOctet: thirdOctet;
            prevOctet: firstOctet;
        }

        IpDot
        {
            parentOctet: secondOctet;
        }

        IpOctet
        {
            id: thirdOctet;
            nextOctet: fourthOctet;
            prevOctet: secondOctet;
        }

        IpDot
        {
            parentOctet: thirdOctet;
        }

        IpOctet
        {
            id: fourthOctet;
            nextOctet: thisComponent.KeyNavigation.tab;
            prevOctet: thirdOctet;
        }
    }
}


