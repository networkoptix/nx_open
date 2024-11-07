// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Core

FisheyeShaderEffect
{
    property MediaResourceHelper helper: null
    property real videoCenterHeightOffsetFactor: 0.0

    fieldOffset: helper
        ? Qt.vector2d(0.5 - helper.fisheyeParams.xCenter, 0.5 - helper.fisheyeParams.yCenter)
        : Qt.vector2d(0.0, 0.0)

    fieldStretch: helper ? helper.fisheyeParams.hStretch : 1.0

    fieldRadius: helper ? helper.fisheyeParams.radius : 0.0

    fieldRotation: helper
        ? (helper.customRotation - helper.fisheyeParams.fovRot)
        : 0.0

    viewRotationMatrix: interactor.animatedRotationMatrix

    viewScale: interactor.animatedScale

    viewShift: width > height
        ? Qt.vector2d(0, 0)
        : Qt.vector2d(0, -(1.0 - width / height) / 2 * videoCenterHeightOffsetFactor)
}
