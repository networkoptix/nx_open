// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "media_output_shader_program.h"

namespace nx::vms::client::desktop {

struct MediaOutputShaderData
{
    MediaOutputShaderProgram::Key key;

    QList<QVector2D> verts;
    QList<QVector2D> texCoords;

    QMatrix3x3 uTexCoordMatrix;
    QMatrix4x4 colorMatrix;
    float gamma = 0.0f;
    float luminocityShift = 0.0f;
    float luminocityFactor = 0.0f;

    QMatrix3x3 texCoordFragMatrix;
    QMatrix3x3 sphereRotationOrUnprojectionMatrix;
    QMatrix3x3 rotationCorrectionMatrix;

    MediaOutputShaderData();
    explicit MediaOutputShaderData(MediaOutputShaderProgram::Key key);
    ~MediaOutputShaderData();

    void initGeometry();
    void loadOpacity(qreal opacity);
    void loadGammaCorrection(const ImageCorrectionResult& parameters);
    void loadDewarpingParameters(
        const api::dewarping::MediaData& mediaParams,
        const api::dewarping::ViewData& itemParams,
        qreal aspectRatio,
        qreal maxX,
        qreal maxY);
};

} // namespace nx::vms::client::desktop
