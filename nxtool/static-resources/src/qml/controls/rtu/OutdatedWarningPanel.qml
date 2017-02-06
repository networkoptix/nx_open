
import QtQuick 2.0;

import "../base" as Base;
import "../../common" as Common;

import networkoptix.rtu 1.0 as NxRtu;

Rectangle
{
    id: thisComponent;

    function addOnUpdateAction(action)
    {
        impl.handlers.push(action);
        show = true;
    }

    property bool show: false;
    property real shownY: 0;
    property real hiddenY: -height;

    enabled: show;
    height: panel.height;
    color: "#35A0DA";

    visible: (state == "visible");
    state: (show ? "visible" : "hidden");
    states:
    [
        State
        {
            name: "visible";
            PropertyChanges { target: thisComponent; y: shownY; }
        }
        , State
        {
            name: "hidden";
            PropertyChanges { target: thisComponent; y: hiddenY; }
        }
    ]

    transitions: Transition
    {
        NumberAnimation
        {
            properties: "y";
            easing.type: Easing.InOutQuad;
            duration: 300;
        }
    }

    Base.Text
    {
        color: "white";
        anchors
        {
            left: parent.left;
            right: panel.left;
            verticalCenter: parent.verticalCenter;

            leftMargin: Common.SizeManager.spacing.large;
        }

        thin: false;
        font.pixelSize: Common.SizeManager.fontSizes.medium;
        text: qsTr("Information outdated");
    }

    Base.ButtonsPanel
    {
        id: panel;

        anchors.right: parent.right;
        buttons: NxRtu.Buttons.Update;
        spacing: Common.SizeManager.spacing.medium;

        onButtonClicked:
        {
            impl.handlers.forEach(function(handler) { handler(); })
            impl.handlers = [];
            thisComponent.show = false;
        }
    }

    property QtObject impl: QtObject
    {
        property var handlers:[];
    }
}
