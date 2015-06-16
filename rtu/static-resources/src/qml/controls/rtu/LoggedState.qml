import QtQuick 2.0;

import "../../common" as Common;

import networkoptix.rtu 1.0 as NxRtu;

Image
{
    id: thisComponent;

    property int loggedState: 0;

    width: Common.SizeManager.clickableSizes.small;
    height: Common.SizeManager.clickableSizes.small;

    source: (loggedState === NxRtu.Constants.LoggedToAllServers ? ""
        : (loggedState === NxRtu.Constants.PartiallyLogged ? "qrc:/resources/lock-open.png"
        : "qrc:/resources/lock-closed.png" ));
}
