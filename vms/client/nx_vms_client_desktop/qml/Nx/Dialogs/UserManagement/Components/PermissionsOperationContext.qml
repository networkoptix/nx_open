// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

NxObject
{
    /** Access right of a hovered cell or a hovered header item. */
    property int accessRight: 0

    /** Model indexes of the selection. */
    property var selectedIndexes: []

    /**
     * If the selection contains both layouts and media resources that are present on those layouts,
     * and `nextCheckState` is Qt.Unchecked, then the "will be inherited from" state computation
     * for those media resources should be aware that the direct access to those layouts is also
     * being lost, so it should be reported to `ResourceAccessRightsModel`.
     */
    property var selectedLayouts: []

    /** Combined check state of `accessRight` of all items in the selection. */
    property int currentCheckState: Qt.Unchecked

    /** Check state after the operation. */
    readonly property int nextCheckState: currentCheckState === Qt.Checked
        ? Qt.Unchecked
        : Qt.Checked

    /** OR-combined granted access rights for all items in the selection. */
    property int currentAccessRights: 0

    /** OR-combined relevant access rights for all items in the selection. */
    property int relevantAccessRights: 0
}
