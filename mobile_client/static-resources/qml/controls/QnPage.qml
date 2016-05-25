import QtQuick 2.4
import Qt.labs.controls 1.0

FocusScope {
    property string title

    property bool activePage: StackView.status === StackView.Active
    property int pageStatus: StackView.status
}
