import QtQuick 2.2
import QtQuick.Window 2.0
import QtQuick.Controls 1.2

import com.networkoptix.qml 1.0

import "dialogs"

Window {
    title: "HD Witness"
    visible: true

    LoginDialog {
        anchors.fill: parent
    }
}
