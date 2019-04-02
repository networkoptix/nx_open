import QtQuick 2.0
import Nx.Controls 1.0

import "private"

ScrollView
{
    id: scrollView

    property string name: ""
    property Item childrenItem: column

    Flickable
    {
        boundsBehavior: Flickable.StopAtBounds

        clip: true

        AlignedColumn
        {
            id: column
            width: scrollView.width
        }
    }
}
