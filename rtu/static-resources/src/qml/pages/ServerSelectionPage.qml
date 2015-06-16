import QtQuick 2.1;
import QtQuick.Controls 1.1;
import QtQuick.Dialogs 1.1;

import "../common" as Common;
import "../controls/rtu" as Rtu;

ScrollView
{
    id: thisComponent;
    
    property alias askForSelectionChange: selectionView.askForSelectionChange;
    
    Rtu.ServerSelectionListView
    {
        id: selectionView;
        
        anchors
        {
            left: parent.left;
            right: parent.right;
        }
    }
}


