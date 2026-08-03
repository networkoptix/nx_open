// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

ObjectsListTile
{
    id: delegate

    // Context properties:
    //  var modelData
    //  ObjectsList objectsList

    objectCount: modelData?.count ?? 0

    tightGroup: delegate.objectCount > 1
        && modelData?.durationMs < objectsList.minimumStackDurationMs

    caption: modelData?.caption ?? ""
    description: modelData?.description ?? ""
    imagePaths: modelData?.imagePaths ?? []
    iconPaths: modelData?.iconPaths ?? []
    objectsType: objectsList.objectsType

    highlighted: !!modelData
        && objectsList.currentPositionMs >= modelData.positionMs
        && objectsList.currentPositionMs < (modelData.positionMs + modelData.durationMs)

    pointerY: mapFromItem(objectsList, 0, objectsList.timeToPosition(modelData?.positionMs ?? 0)).y

    maxCountToDisplay: objectsList.maxObjectsPerBucket
    skeletonController: objectsList.skeletonController
}
