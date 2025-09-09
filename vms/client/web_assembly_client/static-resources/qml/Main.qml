// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

Window {
  visible: true
  title: qsTr("Hello World")
  visibility: Window.FullScreen

  Rectangle {
    anchors.fill: parent
    width: 400
    height: 400
    color: "lightgrey"
    Text {
      anchors.centerIn: parent
      text: "Hello Nx VMS Web Assembly Client\nApplication full version: " + applicationFullVersion
    }
  }
}
