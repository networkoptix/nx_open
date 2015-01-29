import QtQuick 2.2
import Material 0.1

import Material.ListItems 0.1 as ListItem

ListView {
    id: listView

    delegate: ListItem.Subtitled {
        text: resourceName
        subText: ipAddress
    }
}
