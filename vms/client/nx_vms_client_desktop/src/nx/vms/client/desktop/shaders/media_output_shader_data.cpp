// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_output_shader_data.h"

#include <nx/utils/log/assert.h>
#include <nx/vms/api/data/dewarping_data.h>
#include <utils/color_space/image_correction.h>

namespace nx::vms::client::desktop {

namespace {

static QMatrix4x4 withOpacity(const QMatrix4x4& sourceMatrix, qreal opacity)
{
    QMatrix4x4 result(sourceMatrix);
    result(3, 3) *= opacity;
    return result;
}

static const QMatrix4x4 kDefaultYuvToRgbMatrix(
    1.0, 0.0, 1.402, 0.0,
    1.0,-0.344,-0.714, 0.0,
    1.0, 1.772, 0.0, 0.0,
    0.0, 0.0, 0.0, 1.0);

static const QMatrix4x4 kYuvEbu(
    1.0000, 1.0000, 1.0000, 0.0000,
    0.0000,-0.3960, 2.0290, 0.0000,
    1.1400,-0.5810, 0.0000, 0.0000,
    0.0000, 0.0000, 0.0000, 1.0000);

static const QMatrix4x4 kYuvBt601(
    1.0000, 1.0000, 1.0000, 0.0000,
    0.0000,-0.3440, 1.7730, 0.0000,
    1.4030,-0.7140, 0.0000, 0.0000,
    0.0000, 0.0000, 0.0000, 1.0000);

static const QMatrix4x4 kYuvBt709(
    1.0000, 1.0000, 1.0000, 0.0000,
    0.0000,-0.1870, 1.8556, 0.0000,
    1.5701,-0.4664, 0.0000, 0.0000,
    0.0000, 0.0000, 0.0000, 1.0000);

static const QMatrix4x4 kYuvSmtp240m(
    1.0000, 1.0000, 1.0000, 0.0000,
    0.0000,-0.2253, 1.8270, 0.0000,
    1.5756,-0.5000, 0.0000, 0.0000,
    0.0000, 0.0000, 0.0000, 1.0000);

QMatrix4x4 rawColorTransformation(MediaOutputShaderProgram::ColorSpace colorSpace)
{
    switch (colorSpace)
    {
        case MediaOutputShaderProgram::ColorSpace::yuvEbu:
            return kYuvEbu;

        case MediaOutputShaderProgram::ColorSpace::yuvBt601:
            return kYuvBt601;

        case MediaOutputShaderProgram::ColorSpace::yuvBt709:
            return kYuvBt709;

        case MediaOutputShaderProgram::ColorSpace::yuvSmtp240m:
            return kYuvSmtp240m;
    }

    NX_ASSERT(false);
    return kYuvEbu;
}

QMatrix4x4 colorTransformation(MediaOutputShaderProgram::ColorSpace colorSpace, bool fullRange)
{
    QMatrix4x4 result(rawColorTransformation(colorSpace));
    result.translate(0.0, -0.5, -0.5);

    if (!fullRange)
    {
        result.scale(255.0 / (235 - 16), 255.0 / (240 - 16), 255.0 / (240 - 16));
        result.translate(-16.0 / 255, -16.0 / 255, -16.0 / 255);
    }

    return result;
}

} // namespace

MediaOutputShaderData::MediaOutputShaderData()
{
    initGeometry();
}

MediaOutputShaderData::MediaOutputShaderData(MediaOutputShaderProgram::Key key):
    key(key)
{
    initGeometry();
}

MediaOutputShaderData::~MediaOutputShaderData()
{
}

void MediaOutputShaderData::initGeometry()
{
    verts.clear();
    texCoords.clear();

    verts << QVector2D(-1, 1);
    verts << QVector2D(1, 1);
    verts << QVector2D(-1, -1);
    verts << QVector2D(1, -1);

    texCoords << QVector2D(0, 0);
    texCoords << QVector2D(1, 0);
    texCoords << QVector2D(0, 1);
    texCoords << QVector2D(1, 1);
}

void MediaOutputShaderData::loadOpacity(qreal opacity)
{
    switch (key.format)
    {
        case MediaOutputShaderProgram::Format::rgb:
            colorMatrix = withOpacity(QMatrix4x4(), opacity);
            break;

        case MediaOutputShaderProgram::Format::nv12:
            colorMatrix = withOpacity(colorTransformation(
                MediaOutputShaderProgram::ColorSpace::yuvEbu, /*fullRange*/ true), opacity);
            break;

        case MediaOutputShaderProgram::Format::yv12:
            colorMatrix = withOpacity(kDefaultYuvToRgbMatrix, opacity);
            break;

        case MediaOutputShaderProgram::Format::yva12:
            colorMatrix = withOpacity(kDefaultYuvToRgbMatrix, opacity);
            break;

        default:
            colorMatrix = withOpacity(QMatrix4x4(), opacity);
    }
}

void MediaOutputShaderData::loadGammaCorrection(const ImageCorrectionResult& parameters)
{
    if (!NX_ASSERT(key.imageCorrection == MediaOutputShaderProgram::YuvImageCorrection::gamma))
        return;

    gamma = parameters.gamma;
    luminocityShift = parameters.bCoeff;
    luminocityFactor = parameters.aCoeff;
}

// TODO: #vkutin Check if maxX and maxY actually work as intended with non-unit texture rect.
void MediaOutputShaderData::loadDewarpingParameters(
    const api::dewarping::MediaData& mediaParams,
    const api::dewarping::ViewData& itemParams,
    qreal aspectRatio,
    qreal maxX,
    qreal maxY)
{
    const bool isDewarpingEnabled = mediaParams.enabled && itemParams.enabled;
    if (!NX_ASSERT(isDewarpingEnabled == key.dewarping))
        return;

    if (!isDewarpingEnabled)
    {
        uTexCoordMatrix = QMatrix3x3();
        return;
    }

    const bool isPanoramic = key.viewProjection
        == MediaOutputShaderProgram::ViewProjection::equirectangular;
    if (!NX_ASSERT(isPanoramic == itemParams.panoFactor > 1))
        return;

    const bool isSpherical = api::dewarping::MediaData::is360VR(key.cameraProjection);
    const bool isHorizontal = isSpherical
        || mediaParams.viewMode == api::dewarping::FisheyeCameraMount::wall;
    const qreal yPos = isHorizontal ? 0.5 : 1.0;

    const auto scaledAspectRatio = aspectRatio / mediaParams.hStretch;

    const auto xAngle = float(isHorizontal ? itemParams.xAngle : 0.0);

    if (isSpherical)
    {
        // 360 VR.

        const float centerX = float(maxX * 0.5);
        const float centerY = float(maxY * 0.5);

        const float texCoordFragMatrixData[9]{
            float(centerX / M_PI), 0.0f, centerX,
            0.0f, float(centerY / M_PI * aspectRatio), centerY,
            0.0f, 0.0f, 1.0f};

        QMatrix4x4 rotationMatrix;
        rotationMatrix.rotate(-mediaParams.sphereAlpha, 0, 0, 1);
        rotationMatrix.rotate(-mediaParams.sphereBeta, 1, 0, 0);
        rotationMatrix.rotate(mediaParams.sphereAlpha, 0, 0, 1);

        texCoordFragMatrix = QMatrix3x3(texCoordFragMatrixData);
        rotationCorrectionMatrix = rotationMatrix.toGenericMatrix<3, 3>();
    }
    else
    {
        // Fisheye.

        const auto cameraRoll = float(qDegreesToRadians(mediaParams.fovRot) -
            (isHorizontal ? 0.0 : (itemParams.xAngle - M_PI)));

        const auto rollSin = sinf(cameraRoll);
        const auto rollCos = cosf(cameraRoll);

        const float cameraRollMatrixData[9]{
            rollCos, -rollSin, 0.0f,
            rollSin, rollCos, 0.0f,
            0.0f, 0.0f, 1.0f};

        const float texCoordFragMatrixData[9]{
            float(maxX * mediaParams.radius), 0.0f, float(maxX * mediaParams.xCenter),
            0.0f, float(maxY * mediaParams.radius * scaledAspectRatio),
                float(maxY * mediaParams.yCenter),
            0.0f, 0.0f, 1.0f};

        texCoordFragMatrix =
            QMatrix3x3(texCoordFragMatrixData) * QMatrix3x3(cameraRollMatrixData);
    }

    if (isPanoramic)
    {
        const auto yAngle = isHorizontal
            ? itemParams.yAngle
            : itemParams.yAngle - itemParams.fov / itemParams.panoFactor / 2.0;

        const float texCoordVertMatrixData[9]{
            float(itemParams.fov / maxX), 0.0f, float(-0.5 * itemParams.fov + xAngle),
            0.0f, float((itemParams.fov / itemParams.panoFactor) / maxY),
                float(-yPos * itemParams.fov / itemParams.panoFactor - yAngle),
            0.0f, 0.0f, 1.0f};

        static const float kVerticalSwizzleMatrix[9] = {
            1.0f, 0.0f, 0.0f,   //< x' = x
            0.0f, 0.0f, 1.0f,   //< y' = z
            0.0f, -1.0f, 0.0f}; //< z' = -y

        uTexCoordMatrix = QMatrix3x3(texCoordVertMatrixData);

        // sphereRotation
        sphereRotationOrUnprojectionMatrix = isHorizontal
            ? QMatrix3x3()
            : QMatrix3x3(kVerticalSwizzleMatrix);
    }
    else
    {
        const auto yAngle = isHorizontal
            ? itemParams.yAngle
            : itemParams.yAngle - M_PI / 2.0 - itemParams.fov / (2.0 * aspectRatio);

        const auto kx = float(2.0 * tan(itemParams.fov / 2.0));
        const auto ky = float(kx / mediaParams.hStretch);

        const auto sinX = sinf(float(xAngle));
        const auto cosX = cosf(float(xAngle));
        const auto sinY = sinf(float(yAngle));
        const auto cosY = cosf(float(yAngle));

        const float unprojectionMatrixData[9]{
            cosX * kx, sinX * sinY * ky, sinX * cosY,
            -sinX * kx, cosX * sinY * ky, cosX * cosY,
            0.0f, cosY * ky, -sinY};

        const float texCoordVertMatrixData[9]{
            1.0f / float(maxX), 0.0f, -0.5f,
            0.0f, 1.0f / float(maxY * scaledAspectRatio), float(-yPos / scaledAspectRatio),
            0.0f, 0.0f, 1.0f};

        uTexCoordMatrix = QMatrix3x3(texCoordVertMatrixData);
        sphereRotationOrUnprojectionMatrix = QMatrix3x3(unprojectionMatrixData);
    }
}

} // namespace nx::vms::client::desktop
