import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx.Dialogs 1.0
import Nx 1.0
import com.networkoptix.qml 1.0

DialogBase
{
    id: testSelectionDialog

    deleteOnClose: true

    Column
    {
        width: parent.width

        DialogTitle
        {
            text: qsTr("Select Test")
        }

        Repeater
        {
            model:
            [
                "LiteClientTest"
            ]

            DialogListItem
            {
                text: modelData
                onClicked:
                {
                    testSelectionDialog.close()
                    Workflow.startTest(modelData)
                }
            }
        }
    }
}
