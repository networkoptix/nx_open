import QtQuick 2.4
import QtQuick.Controls 1.3

FocusScope {
    property string title

    property bool activePage: Stack.status === Stack.Active
    property int pageStatus: Stack.status
}
