// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_output_shader_program.h"

#include <QtCore/QHashFunctions>
#include <QtCore/qmath.h>
#include <QtGui/QMatrix4x4>
#include <QtGui/QOpenGLContext>

#include <utils/color_space/image_correction.h>

#include <nx/vms/api/data/dewarping_data.h>
#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

namespace {

using namespace nx::vms::api::dewarping;

QString glType()
{
    // TODO: #vkutin Uncomment this and make GLES version of the shaders if it is ever required.
    // if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES)
    //     return "es";

    const auto currentContext = QOpenGLContext::currentContext();
    if (NX_ASSERT(currentContext)
        && currentContext->format().profile() == QSurfaceFormat::CoreProfile)
    {
        return "core";
    }

    return "compatibility";
}

QString shaderPath(const QString& localPath)
{
    return QStringLiteral(":/shaders/%1/media_output/%2").arg(glType(), localPath);
}

void addShader(
    QOpenGLShaderProgram* program, QOpenGLShader::ShaderType type, const QString& localPath)
{
    program->addShaderFromSourceFile(type, shaderPath(localPath));
}

void addVertexShader(QOpenGLShaderProgram* program, const QString& localPath)
{
    addShader(program, QOpenGLShader::Vertex, localPath);
}

void addFragmentShader(QOpenGLShaderProgram* program, const QString& localPath)
{
    addShader(program, QOpenGLShader::Fragment, localPath);
}

QMatrix4x4 withOpacity(const QMatrix4x4& sourceMatrix, qreal opacity)
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

bool MediaOutputShaderProgram::Key::operator==(const Key& other) const
{
    return format == other.format
        && imageCorrection == other.imageCorrection
        && dewarping == other.dewarping
        && (!dewarping
            || (viewProjection == other.viewProjection && cameraProjection == other.cameraProjection));
}

size_t qHash(const MediaOutputShaderProgram::Key& key, uint seed)
{
    const auto combine = QtPrivate::QHashCombine();

    const int lp = key.dewarping ? int(key.viewProjection) : 0;
    const int vp = key.dewarping ? int(key.viewProjection) : 0;

    return combine(combine(combine(combine(combine(seed,
        ::qHash(int(key.format))),
        ::qHash(int(key.imageCorrection))),
        ::qHash(key.dewarping)),
        ::qHash(lp)),
        ::qHash(vp));
}

size_t MediaOutputShaderProgram::planeCount(AVPixelFormat format)
{
    switch (format)
    {
        case AV_PIX_FMT_RGBA:
            return 1;
        case AV_PIX_FMT_NV12:
            return 2;
        case AV_PIX_FMT_YUV420P:
        case AV_PIX_FMT_YUV422P:
        case AV_PIX_FMT_YUV444P:
            return 3;
        case AV_PIX_FMT_YUVA420P:
            return 4;
        default:
            return 0; // Other formats must be converted before picture uploading.
    }
}

MediaOutputShaderProgram::Format MediaOutputShaderProgram::formatFromPlaneCount(int planeCount)
{
    switch (planeCount)
    {
        case 1: return MediaOutputShaderProgram::Format::rgb;
        case 2: return MediaOutputShaderProgram::Format::nv12;
        case 3: return MediaOutputShaderProgram::Format::yv12;
        case 4: return MediaOutputShaderProgram::Format::yva12;
        default: return {};
    }
}

MediaOutputShaderProgram::MediaOutputShaderProgram(const Key& key, QObject* parent):
    base_type(parent),
    m_key(key)
{
    addVertexShader(this, "main.vert");
    addFragmentShader(this, "main.frag");

    bool supportsYuvImageCorrection = true;
    switch (key.format)
    {
        case Format::yv12:
            addFragmentShader(this, "formats/yv12.frag");
            break;

        case Format::nv12:
            addFragmentShader(this, "formats/nv12.frag");
            break;

        case Format::yva12:
            addFragmentShader(this, "formats/yva12.frag");
            break;

        case Format::rgb:
            addFragmentShader(this, "formats/rgb.frag");
            supportsYuvImageCorrection = false;
            break;
    }

    if (supportsYuvImageCorrection)
    {
        switch (key.imageCorrection)
        {
            case YuvImageCorrection::none:
                addFragmentShader(this, "color_correction/none.frag");
                break;

            case YuvImageCorrection::gamma:
                addFragmentShader(this, "color_correction/yuv_gamma.frag");
                break;
        }
    }
    else
    {
        NX_ASSERT(key.imageCorrection == YuvImageCorrection::none);
        addFragmentShader(this, "color_correction/none.frag");
    }

    if (key.dewarping)
    {
        addFragmentShader(this, "dewarping.frag");

        switch (key.viewProjection)
        {
            case ViewProjection::rectilinear:
                addFragmentShader(this, "projection/rectilinear.frag");
                break;

            case ViewProjection::equirectangular:
                addFragmentShader(this, "projection/equirectangular.frag");
                break;
        }

        switch (key.cameraProjection)
        {
            case CameraProjection::equidistant:
                addFragmentShader(this, "camera/equidistant.frag");
                break;

            case CameraProjection::stereographic:
                addFragmentShader(this, "camera/stereographic.frag");
                break;

            case CameraProjection::equisolid:
                addFragmentShader(this, "camera/equisolid.frag");
                break;

            case CameraProjection::equirectangular360:
                addFragmentShader(this, "camera/equirectangular360.frag");
                break;
        }
    }
    else
    {
        addFragmentShader(this, "no_texture_transform.frag");
    }
}

void MediaOutputShaderProgram::loadRgbParameters(int rgbTexture, qreal opacity)
{
    if (!NX_ASSERT(m_key.format == Format::rgb))
        return;

    setUniformValue("rgbTexture", rgbTexture);
    setUniformValue("colorMatrix", withOpacity(QMatrix4x4(), opacity));
}

void MediaOutputShaderProgram::loadYv12Parameters(
    int yTexture, int uTexture, int vTexture, qreal opacity)
{
    if (!NX_ASSERT(m_key.format == Format::yv12))
        return;

    setUniformValue("yTexture", yTexture);
    setUniformValue("uTexture", uTexture);
    setUniformValue("vTexture", vTexture);
    setUniformValue("colorMatrix", withOpacity(kDefaultYuvToRgbMatrix, opacity));
}

void MediaOutputShaderProgram::loadYva12Parameters(
    int yTexture, int uTexture, int vTexture, int aTexture, qreal opacity)
{
    if (!NX_ASSERT(m_key.format == Format::yva12))
        return;

    setUniformValue("yTexture", yTexture);
    setUniformValue("uTexture", uTexture);
    setUniformValue("vTexture", vTexture);
    setUniformValue("aTexture", aTexture);
    setUniformValue("colorMatrix", withOpacity(kDefaultYuvToRgbMatrix, opacity));
}

void MediaOutputShaderProgram::loadNv12Parameters(int yTexture, int uvTexture, qreal opacity,
    ColorSpace colorSpace, bool fullRange)
{
    if (!NX_ASSERT(m_key.format == Format::nv12))
        return;

    setUniformValue("yTexture", yTexture);
    setUniformValue("uvTexture", uvTexture);

    setUniformValue("colorMatrix",
        withOpacity(colorTransformation(colorSpace, fullRange), opacity));
}

void MediaOutputShaderProgram::loadGammaCorrection(const ImageCorrectionResult& parameters)
{
    if (!NX_ASSERT(m_key.imageCorrection == YuvImageCorrection::gamma))
        return;

    setUniformValue("gamma", GLfloat(parameters.gamma));
    setUniformValue("luminocityShift", GLfloat(parameters.bCoeff));
    setUniformValue("luminocityFactor", GLfloat(parameters.aCoeff));
}

// TODO: #vkutin Check if maxX and maxY actually work as intended with non-unit texture rect.
void MediaOutputShaderProgram::loadDewarpingParameters(const MediaData& mediaParams,
    const ViewData& itemParams, qreal aspectRatio, qreal maxX, qreal maxY)
{
    const bool isDewarpingEnabled = mediaParams.enabled && itemParams.enabled;
    if (!NX_ASSERT(isDewarpingEnabled == m_key.dewarping))
        return;

    if (!isDewarpingEnabled)
    {
        setUniformValue("uTexCoordMatrix", QMatrix3x3());
        return;
    }

    const bool isPanoramic = m_key.viewProjection == ViewProjection::equirectangular;
    if (!NX_ASSERT(isPanoramic == itemParams.panoFactor > 1))
        return;

    const bool isSpherical = MediaData::is360VR(m_key.cameraProjection);
    const bool isHorizontal = isSpherical || mediaParams.viewMode == FisheyeCameraMount::wall;
    const qreal yPos = isHorizontal ? 0.5 : 1.0;

    const auto scaledAspectRatio = aspectRatio / mediaParams.hStretch;

    const auto xAngle = float(isHorizontal ? itemParams.xAngle : 0.0);

    if (isSpherical)
    {
        // 360 VR.

        const float centerX = float(maxX * 0.5);
        const float centerY = float(maxY * 0.5);

        const float texCoordFragMatrix[9]{
            float(centerX / M_PI), 0.0f, centerX,
            0.0f, float(centerY / M_PI * aspectRatio), centerY,
            0.0f, 0.0f, 1.0f};

        QMatrix4x4 rotationMatrix;
        rotationMatrix.rotate(-mediaParams.sphereAlpha, 0, 0, 1);
        rotationMatrix.rotate(-mediaParams.sphereBeta, 1, 0, 0);
        rotationMatrix.rotate(mediaParams.sphereAlpha, 0, 0, 1);

        setUniformValue("texCoordFragMatrix", QMatrix3x3(texCoordFragMatrix));
        setUniformValue("rotationCorrectionMatrix", rotationMatrix.toGenericMatrix<3, 3>());
    }
    else
    {
        // Fisheye.

        const auto cameraRoll = float(qDegreesToRadians(mediaParams.fovRot) -
            (isHorizontal ? 0.0 : (itemParams.xAngle - M_PI)));

        const auto rollSin = sinf(cameraRoll);
        const auto rollCos = cosf(cameraRoll);

        const float cameraRollMatrix[9]{
            rollCos, -rollSin, 0.0f,
            rollSin, rollCos, 0.0f,
            0.0f, 0.0f, 1.0f};

        const float texCoordFragMatrix[9]{
            float(maxX * mediaParams.radius), 0.0f, float(maxX * mediaParams.xCenter),
            0.0f, float(maxY * mediaParams.radius * scaledAspectRatio),
                float(maxY * mediaParams.yCenter),
            0.0f, 0.0f, 1.0f};

        setUniformValue("texCoordFragMatrix",
            QMatrix3x3(texCoordFragMatrix) * QMatrix3x3(cameraRollMatrix));
    }

    if (isPanoramic)
    {
        const auto yAngle = isHorizontal
            ? itemParams.yAngle
            : itemParams.yAngle - itemParams.fov / itemParams.panoFactor / 2.0;

        const float texCoordVertMatrix[9]{
            float(itemParams.fov / maxX), 0.0f, float(-0.5 * itemParams.fov + xAngle),
            0.0f, float((itemParams.fov / itemParams.panoFactor) / maxY),
                float(-yPos * itemParams.fov / itemParams.panoFactor - yAngle),
            0.0f, 0.0f, 1.0f};

        static const float kVerticalSwizzleMatrix[9] = {
            1.0f, 0.0f, 0.0f,   //< x' = x
            0.0f, 0.0f, 1.0f,   //< y' = z
            0.0f, -1.0f, 0.0f}; //< z' = -y

        setUniformValue("uTexCoordMatrix", QMatrix3x3(texCoordVertMatrix));

        setUniformValue("sphereRotationMatrix", isHorizontal
            ? QMatrix3x3()
            : QMatrix3x3(kVerticalSwizzleMatrix));
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

        const float unprojectionMatrix[9]{
            cosX * kx, sinX * sinY * ky, sinX * cosY,
            -sinX * kx, cosX * sinY * ky, cosX * cosY,
            0.0f, cosY * ky, -sinY};

        const float texCoordVertMatrix[9]{
            1.0f / float(maxX), 0.0f, -0.5f,
            0.0f, 1.0f / float(maxY * scaledAspectRatio), float(-yPos / scaledAspectRatio),
            0.0f, 0.0f, 1.0f};

        setUniformValue("uTexCoordMatrix", QMatrix3x3(texCoordVertMatrix));
        setUniformValue("unprojectionMatrix", QMatrix3x3(unprojectionMatrix));
    }
}

} // namespace nx::vms::client::desktop
