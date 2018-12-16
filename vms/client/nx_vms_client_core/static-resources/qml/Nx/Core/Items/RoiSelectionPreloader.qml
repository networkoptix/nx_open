import QtQuick 2.6
import QtGraphicalEffects 1.0

Item
{
    id: item

    property color mainColor
    property color shadowColor

    property point centerPoint

    property real dashSize: 12

    property real normalSize: (dashSize + normalCircleRadius) * 2
    property real normalCircleRadius: 5.5
    property real normalThickness: 1

    property bool enableAnimation: true
    property int animationDuration: 600
    property real expandedSize: 63
    property real expandedCircleRadius: expandedSize / 2
    property real expandedThickness: 3
    property real centerCircleRadius: 2.5

    layer.enabled: true

    x: centerPoint.x - width / 2
    y: centerPoint.y - height / 2

    width: d.size
    height: width
    visible: opacity > 0

    Item
    {
        id: visualHolder

        anchors.fill: parent
        visible: false

        Rectangle
        {
            id: leftDash

            anchors.verticalCenter: parent.verticalCenter
            x: d.thickness
            color: item.mainColor
            width: item.dashSize - d.thickness
            height: d.thickness
        }

        Rectangle
        {
            id: rightDash

            anchors.verticalCenter: parent.verticalCenter
            x: parent.width - width - d.thickness
            color: item.mainColor
            width: item.dashSize - d.thickness
            height: d.thickness
        }

        Rectangle
        {
            id: topDash

            anchors.horizontalCenter: parent.horizontalCenter
            y: d.thickness
            color: item.mainColor
            width: d.thickness
            height: item.dashSize - d.thickness
        }

        Rectangle
        {
            id: bottomDash

            anchors.horizontalCenter: parent.horizontalCenter
            y: parent.height - height - d.thickness
            color: item.mainColor
            width: d.thickness
            height: item.dashSize - d.thickness
        }

        Circle
        {
            id: circle

            anchors.centerIn: parent
            borderColor: item.mainColor
            radius: d.circleRadius
            border.width: d.thickness
        }

        Circle
        {
            id: centerCircle

            anchors.centerIn: parent
            circleColor: item.mainColor
            radius: item.centerCircleRadius
        }
    }

    DropShadow
    {
        id: shadow

        anchors.fill: visualHolder
        radius: 1
        color: item.shadowColor
        source: visualHolder
    }

    states:
    [
        State
        {
            name: "hidden"

            PropertyChanges { target: item; opacity: 0 }
            PropertyChanges { target: centerCircle; visible: false }
            PropertyChanges
            {
                target: d
                size: item.normalSize
                thickness: item.normalThickness
                circleRadius: item.normalCircleRadius
            }
        },
        State
        {
            name: "normal"

            PropertyChanges { target: item; opacity: 1 }
            PropertyChanges { target: centerCircle; visible: false }
            PropertyChanges
            {
                target: d
                size: item.normalSize
                thickness: item.normalThickness
                circleRadius: item.normalCircleRadius
            }
        },
        State
        {
            name: "expanded"

            PropertyChanges { target: item; opacity: 0.4 }
            PropertyChanges { target: centerCircle; visible: true }
            PropertyChanges
            {
                target: d
                size: item.expandedSize
                thickness: item.expandedThickness
                circleRadius: item.expandedCircleRadius
            }
        }
    ]

    QtObject
    {
        id: d

        readonly property int animationDuration: item.enableAnimation ? item.animationDuration : 0
        property real size: item.normalSize
        property real thickness: item.normalThickness
        property real circleRadius: item.normalCircleRadius

        Behavior on circleRadius
        {
            NumberAnimation
            {
                duration: d.animationDuration
                easing.type: Easing.OutCubic
            }
        }

        Behavior on thickness
        {
            NumberAnimation
            {
                duration: d.animationDuration
                easing.type: Easing.OutCubic
            }
        }

        Behavior on size
        {
            NumberAnimation
            {
                duration: d.animationDuration
                easing.type: Easing.OutCubic
            }
        }
    }

    Behavior on opacity
    {
        NumberAnimation
        {
            duration: d.animationDuration
            easing.type: Easing.OutCubic
        }
    }
}
