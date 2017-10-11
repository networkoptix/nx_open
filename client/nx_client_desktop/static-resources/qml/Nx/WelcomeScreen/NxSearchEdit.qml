import QtQuick 2.6;
import Nx.WelcomeScreen 1.0;

NxTextEdit
{
    id: control;

    property int delay: 250;
    property real targetWidth: 250;

    property var visualParent;

    property string query;

    backgroundColor: Style.colors.window;
    activeColor: Style.getPaletteColor("dark", 2);
    textControlEnabled: (state != "masked");

    state: "masked";
    width: 280;

    function clear()
    {
        text = "";
        state = "masked";
        animation.complete();
    }

    leftControlDelegate: Image
    {
        source: "qrc:/skin/welcome_page/search.png";
    }

    rightControlDelegate: NxImageButton
    {
        visible: text.length;
        standardIconUrl: "qrc:/skin/welcome_page/input_clear.png";
        hoveredIconUrl: "qrc:/skin/welcome_page/input_clear_hovered.png";
        pressedIconUrl: "qrc:/skin/welcome_page/input_clear_pressed.png";

        onClicked:
        {
            control.text = "";
            control.forceActiveFocus();
        }
    }

    onVisibleChanged:
    {
        if (enabled && visible)
            forceActiveFocus();
    }

    onTextChanged: { timer.restart(); }

    NxLabel
    {
        id: searchLabel;

        text: qsTr("Search");

        anchors.verticalCenter: parent.verticalCenter;
        x: (parent.width - width) / 2;
        visible: (opacity != 0 && !control.text.length);
    }

    MouseArea
    {
        id: activationArea;
        anchors.fill: parent;

        hoverEnabled: true;
        visible: (control.state == "masked");
        acceptedButtons: (control.state == "editable" ? Qt.NoButton : Qt.AllButtons);
        onClicked:
        {
            control.forceActiveFocus();
            control.state = "editable";
        }
    }

    MouseArea
    {
        id: cancelArea;

        anchors.fill: parent;
        parent: (visualParent ? visualParent : control.parent);

        visible: control.visible && control.state == "editable";

        onPressed:
        {
            if (!control.text.length)
                control.state = "masked";

            mouse.accepted = false;
        }
    }

    Rectangle
    {
        x: -1;
        y: 0;
        width: parent.width + 2;
        height: parent.height;

        color: "#00000000";
        radius: 1;
        border.color: (control.hasFocus || activationArea.containsMouse
            ? Style.colors.mid
            : Style.colors.dark)
    }


    states:
    [
        State
        {
            name: "editable";

            PropertyChanges
            {
                target: searchLabel;
                x: leftControl.width
                    + 8; //< Offset for cursor position
            }
        },

        State
        {
            name: "masked";

            PropertyChanges
            {
                target: searchLabel;
                x: (control.width - searchLabel.width) / 2;
            }
        }
    ]

    transitions: Transition
    {
        SequentialAnimation
        {
            id: animation;

            NumberAnimation
            {
                target: searchLabel;
                property: "x";
                duration: 200;
                easing.type: Easing.OutCubic;
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

    Timer
    {
        id: timer;

        repeat: false;
        interval: control.delay;
        onTriggered: { control.query = control.text; }
    }
}
