// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtGui/QMatrix4x4>
#include <QtGui/QVector2D>

#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    #include <rhi/qrhi.h>
#else
    #include <QtGui/private/qrhi_p.h>
#endif

namespace nx::vms::common::pixelation {

/**
 * Shader matrix representation.
 */
struct ShaderMatrix
{
    float mvp[4 * 4];
    void set(const QMatrix4x4& matrix);
};

/**
 * Shader color representation.
 */
struct ShaderColor
{
    float color[4];
    void set(float r, float g, float b, float a);
};

static_assert(sizeof(QVector2D) == sizeof(float) * 2);

template<typename T> inline constexpr size_t kStd140Alignment = alignof(T);
template<> inline constexpr size_t kStd140Alignment<QVector2D> = 8;
template<> inline constexpr size_t kStd140Alignment<ShaderMatrix> = 16;
template<> inline constexpr size_t kStd140Alignment<ShaderColor> = 16;

/**
 * Makes a variable aligned as required by std140.
 */
template<typename T>
struct Std140Aligned
{
    alignas(kStd140Alignment<T>) T value;

    Std140Aligned() = default;
    Std140Aligned(const T& value): value(value) {}

    T* operator->() { return &value; }
    const T* operator->() const { return &value; }
};

/**
 * Color shader used to draw a mask for later use during pixelation. Shader sources:
 * - nx/vms/common/pixelation/shaders/color.vert
 * - nx/vms/common/pixelation/shaders/color.frag
 */
class ColorShader
{
public:
    /**
     * Represents the shader data structure in a buffer.
     */
    struct ShaderData
    {
        Std140Aligned<ShaderMatrix> matrix;
        Std140Aligned<ShaderColor> color;
    };

public:
    ColorShader();

    /**
     * Binds resources to the shader.
     * @param dataBuffer ShaderData buffer.
     */
    void bindResources(QRhiBuffer* dataBuffer);

    /**
     * Sets the vertex input layout.
     */
    void setVertexInputLayout(
        uint32_t stride,
        uint32_t vertexOffset,
        QRhiVertexInputAttribute::Format vertexFormat);

    /**
     * Binds shaders, resources and the input layout to pipeline.
     */
    void bindToPipeline(QRhiGraphicsPipeline* pipeline);

private:
    QShader m_vertexShader;
    QShader m_fragmentShader;
    std::unique_ptr<QRhiShaderResourceBindings> m_shaderResourceBindings;
    QRhiVertexInputLayout m_inputLayout;
};

/**
 * Pixelation shader uses a mask texture to pixelate a source texture. Shader sources:
 * - nx/vms/common/pixelation/shaders/pixelation.vert
 * - nx/vms/common/pixelation/shaders/pixelation.frag
 */
class PixelationShader
{
public:
    /**
     * Represents the shader data structure in a buffer.
     */
    struct ShaderData
    {
        Std140Aligned<ShaderMatrix> matrix;
        Std140Aligned<QVector2D> textureOffset;
        Std140Aligned<int32_t> isHorizontalPass = 0;
    };

public:
    PixelationShader();

    /**
     * Binds resources to the shader.
     * @param dataBuffer ShaderData buffer.
     * @param m_sourceTexture Texture to pixelate.
     * @param m_sourceSampler Source texture sampler.
     * @param m_maskTexture Pixelation mask texture.
     * @param m_maskSampler Pixelation mask texture sampler.
     */
    void bindResources(
        QRhiBuffer* dataBuffer,
        QRhiTexture* m_sourceTexture,
        QRhiSampler* m_sourceSampler,
        QRhiTexture* m_maskTexture,
        QRhiSampler* m_maskSampler);

    /**
     * Sets the vertex input layout.
     */
    void setupVertexInputLayout(
        uint32_t stride,
        uint32_t vertexOffset,
        QRhiVertexInputAttribute::Format vertexFormat);

    /**
     * Binds shaders, resources and the input layout to pipeline.
     */
    void bindToPipeline(QRhiGraphicsPipeline* pipeline);

private:
    QShader m_vertexShader;
    QShader m_fragmentShader;
    std::unique_ptr<QRhiShaderResourceBindings> m_shaderResourceBindings;
    QRhiVertexInputLayout m_inputLayout;
};

} // nx::vms::common::pixelation
