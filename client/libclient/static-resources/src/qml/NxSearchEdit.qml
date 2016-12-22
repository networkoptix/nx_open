import QtQuick 2.6;

import "."

NxTextEdit
{
    id: control;

    property int delay: 250;
    property real targetWidth: 250;

    property var visualParent;

    property string query;

    backgroundColor: activeColor;
    activeColor: Style.darkerColor(Style.colors.shadow, 2)
    textControlEnabled: (state != "masked");

    state: "masked";
    width: mask.width;

    leftControlDelegate: Image
    {
        source: "qrc:/skin/welcome_page/search.png";
    }

    rightControlDelegate: (text.length ? cleanTextControl : null);

    onVisibleChanged:
    {
        if (enabled && visible)
            forceActiveFocus();
    }

    onTextChanged: { timer.restart(); }


    MouseArea
    {
        id: cancelArea;

        property bool cursorIsOutsideArea:
        {
            if (!containsMouse)
                return false;

            var editAreaPos = cancelArea.mapToItem(control, mouseX, mouseY);
            return ((editAreaPos.x < 0) || (editAreaPos.y < 0)
                || (editAreaPos.x > control.width) || (editAreaPos.y > control.height));
        }

        parent: control.visualParent;
        anchors.fill: parent;

        visible: control.visible && control.state == "editable";

        hoverEnabled: true;
        cursorShape: (cursorIsOutsideArea ? Qt.ArrowCursor : Qt.IBeamCursor);
        onPressed:
        {
            if (!control.text.length && cancelArea.cursorIsOutsideArea)
                control.state = "masked";

            mouse.accepted = false;
        }
    }

    Item
    {
        id: mask;

        width: row.width;
        height: row.height;
        anchors.centerIn: parent;

        Rectangle
        {
            id: maskBackground;
            color: Style.colors.window;
            anchors.fill: parent;
            anchors.margins: -2;

            visible: (searchCaption.opacity == 1);
        }

        Row
        {
            id: row;

            Image
            {
                source: "qrc:/skin/welcome_page/search.png";
                opacity: (searchCaption.opacity == 1 ? 1 : 0);
            }

            NxLabel
            {
                id: searchCaption;
                text: qsTr("Search");
                anchors.verticalCenter: parent.verticalCenter;
            }
        }

        MouseArea
        {
            anchors.fill: parent;

            visible: (control.state == "masked");
            cursorShape: Qt.PointingHandCursor;
            acceptedButtons: (control.state == "editable" ? Qt.NoButton : Qt.AllButtons);
            onClicked:
            {
                control.forceActiveFocus();
                control.state = "editable";
            }
        }
    }

    states:
    [
        State
        {
            name: "editable";

            PropertyChanges
            {
                target: control;
                width: control.targetWidth;
            }

            PropertyChanges
            {
                target: searchCaption;
                opacity: 0;
            }
        },

        State
        {
            name: "masked";

            PropertyChanges
            {
                target: control;
                width: mask.width;
            }

            PropertyChanges
            {
                target: searchCaption;
                opacity: 1;
            }
        }
    ]

    transitions: Transition
    {
        SequentialAnimation
        {
            ParallelAnimation
            {
                NumberAnimation
                {
                    target: control;
                    property: "width";
                    duration: 200;
                    easing.type: Easing.InOutCubic;
                }

                NumberAnimation
                {
                    target: searchCaption;
                    property: "opacity";
                    duration: 200;
                    easing.type: Easing.InOutCubic;
                }
            }

            ScriptAction
            {
                script:
                {
                    if (control.state == "editable")
                        control.forceActiveFocus();
                    else
                        control.focus = false;
                }
            }
        }
    }

    Component
    {
        id: cleanTextControl
        NxImageButton
        {
            standardIconUrl: "qrc:/skin/welcome_page/input_clear.png";
            hoveredIconUrl: "qrc:/skin/welcome_page/input_clear_hovered.png";
            pressedIconUrl: "qrc:/skin/welcome_page/input_clear_pressed.png";

            onClicked:
            {
                control.text = "";
                control.forceActiveFocus();
            }
        }
    }

    Timer
    {
        id: timer;

        repeat: false;
        interval: control.delay;
        onTriggered: { control.query = control.text; }
    }
}
