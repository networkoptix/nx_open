import QtQuick 2.2

import com.networkoptix.qml 1.0

import "../items"

Item {
    id: resourcesDialog

    property int __itemsPerRow: 2

    property var __syspal: SystemPalette {
        colorGroup: SystemPalette.Active
    }

    Rectangle {
        anchors.fill: parent
        color: __syspal.window
    }

    QnPlainResourceModel {
        id: resourceModel
    }

    Flickable {
        id: flickable
        anchors.fill: parent

        contentWidth: parent.width
        contentHeight: flow.childrenRect.height

        Flow {
            id: flow
            width: parent.width

            Repeater {
                id: repeater

                model: resourceModel

                delegate: QnResourceItemDelegate {
                    name: display
                    type: nodeType

                    width: {
                        if (nodeType == 0)
                            return flickable.width
                        else
                            return flickable.width / __itemsPerRow
                    }
                }
            }
        }
    }
}
