import Nx.Controls 1.0
import nx.vms.client.desktop 1.0

Button
{
    id: control

    property string name: ""
    property alias caption: control.text
    property string description: ""

    GlobalToolTip.text: description
}
