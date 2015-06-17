import QtQuick 2.4

Item {
    id: icon

    property color color: "white"

    width: dp(24)
    height: dp(24)

    Rectangle {
      id: bar1
      x: dp(2)
      y: dp(5)
      width: dp(20)
      height: dp(2)
      antialiasing: true
      color: icon.color
    }

    Rectangle {
      id: bar2
      x: dp(2)
      y: dp(10)
      width: dp(20)
      height: dp(2)
      antialiasing: true
      color: icon.color
    }

    Rectangle {
      id: bar3
      x: dp(2)
      y: dp(15)
      width: dp(20)
      height: dp(2)
      antialiasing: true
      color: icon.color
    }

    Component.onCompleted: console.log(bar1.y + " " + bar2.y + " " + bar3.y)
}
