import Nx.Core 1.0

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
        ? (helper.fisheyeParams.fovRot + helper.customRotation)
        : 0.0

    viewRotationMatrix: interactor.animatedRotationMatrix

    viewScale: interactor.animatedScale

    viewShift: width > height
        ? Qt.vector2d(0, 0)
        : Qt.vector2d(0, -(1.0 - width / height) / 2 * videoCenterHeightOffsetFactor)
}
