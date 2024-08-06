// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/types/dewarping_types.h>
#include <ui/graphics/shaders/base_shader_program.h>

extern "C" {
#include <libavutil/pixfmt.h>
}

struct ImageCorrectionResult;

namespace nx::vms::api::dewarping {

struct MediaData;
struct ViewData;

} // namespace nx::vms::api

namespace nx::vms::client::desktop {

class MediaOutputShaderProgram: public QnGLShaderProgram
{
    using base_type = QnGLShaderProgram;

public:
    enum class Format
    {
        rgb,
        yv12,
        nv12,
        yva12
    };

    enum class YuvImageCorrection
    {
        none,
        gamma
    };

    enum class ColorSpace
    {
        yuvEbu,
        yuvBt601,
        yuvBt709,
        yuvSmtp240m
    };

    using CameraProjection = nx::vms::api::dewarping::CameraProjection;
    using ViewProjection = nx::vms::api::dewarping::ViewProjection;

    struct Key
    {
        Format format = Format::rgb;
        YuvImageCorrection imageCorrection = YuvImageCorrection::none;
        bool dewarping = false;
        CameraProjection cameraProjection = CameraProjection::equidistant;
        ViewProjection viewProjection = ViewProjection::rectilinear;

        bool operator==(const Key& other) const;
        bool operator!=(const Key& other) const { return !(*this == other); }
    };

    // Should be constructed and destructed when relevant OpenGL context is current.
    explicit MediaOutputShaderProgram(const Key& key, QObject* parent = nullptr);
    virtual ~MediaOutputShaderProgram() override = default;

    const Key& key() const { return m_key; }

    void loadRgbParameters(int rgbTexture, qreal opacity);
    void loadYv12Parameters(int yTexture, int uTexture, int vTexture, qreal opacity);
    void loadYva12Parameters(int yTexture, int uTexture, int vTexture, int aTexture, qreal opacity);

    void loadNv12Parameters(int yTexture, int uvTexture, qreal opacity,
        ColorSpace colorSpace = ColorSpace::yuvEbu, bool fullRange = true);

    void loadGammaCorrection(const ImageCorrectionResult& parameters);

    void loadDewarpingParameters(const nx::vms::api::dewarping::MediaData& mediaParams,
        const nx::vms::api::dewarping::ViewData& itemParams, qreal aspectRatio, qreal maxX,
        qreal maxY);

    static size_t planeCount(AVPixelFormat format);
    static Format formatFromPlaneCount(int planeCount);

private:
    const Key m_key;
};

size_t qHash(const MediaOutputShaderProgram::Key& key, uint seed = 0);

} // namespace nx::vms::client::desktop
