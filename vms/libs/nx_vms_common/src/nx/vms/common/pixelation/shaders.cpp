// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "shaders.h"

#include <QtCore/QFile>

#include <nx/utils/log/assert.h>

namespace nx::vms::common::pixelation {
namespace {

QShader load(const QString &name)
{
    QFile file(name);
    const bool isOpened = file.open(QIODevice::ReadOnly);
    return NX_ASSERT(isOpened) ? QShader::fromSerialized(file.readAll()) : QShader{};
};

} // namespace

void ShaderMatrix::set(const QMatrix4x4 &matrix)
{
    std::memcpy(mvp, matrix.constData(), sizeof(mvp));
}

void ShaderColor::set(float r, float g, float b, float a)
{
    color[0] = r;
    color[1] = g;
    color[2] = b;
    color[3] = a;
}

ColorShader::ColorShader()
{
    m_vertexShader = load(":/src/nx/vms/common/pixelation/shaders/color.vert.qsb");
    m_fragmentShader = load(":/src/nx/vms/common/pixelation/shaders/color.frag.qsb");
}

void ColorShader::bindResources(QRhiBuffer* dataBuffer)
{
    m_shaderResourceBindings.reset(dataBuffer->rhi()->newShaderResourceBindings());
    m_shaderResourceBindings->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(
            /*binding*/ 0,
            QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            dataBuffer)
    });
    m_shaderResourceBindings->create();
}

void ColorShader::bindToPipeline(QRhiGraphicsPipeline* pipeline)
{
    if (!NX_ASSERT(
        m_vertexShader.isValid() && m_fragmentShader.isValid() && m_shaderResourceBindings))
    {
        return;
    }

    pipeline->setShaderStages({
        {QRhiShaderStage::Vertex,  m_vertexShader},
        {QRhiShaderStage::Fragment, m_fragmentShader}
    });

    pipeline->setVertexInputLayout(m_inputLayout);
    pipeline->setShaderResourceBindings(m_shaderResourceBindings.get());
}

void ColorShader::setVertexInputLayout(
    uint32_t stride, uint32_t vertexOffset, QRhiVertexInputAttribute::Format vertexFormat)
{
    m_inputLayout.setBindings({
        QRhiVertexInputBinding{stride}
    });
    m_inputLayout.setAttributes({
        QRhiVertexInputAttribute{/*binding*/ 0, /*location*/ 0, vertexFormat, vertexOffset}
    });
}

PixelationShader::PixelationShader()
{
    m_vertexShader = load(":/src/nx/vms/common/pixelation/shaders/pixelation.vert.qsb");
    m_fragmentShader = load(":/src/nx/vms/common/pixelation/shaders/pixelation.frag.qsb");
}

void PixelationShader::bindResources(
    QRhiBuffer* dataBuffer,
    QRhiTexture* m_sourceTexture,
    QRhiSampler* m_sourceSampler,
    QRhiTexture* m_maskTexture,
    QRhiSampler* m_maskSampler)
{
    m_shaderResourceBindings.reset(dataBuffer->rhi()->newShaderResourceBindings());
    m_shaderResourceBindings->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(
            /*binding*/ 0,
            QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            dataBuffer),

        QRhiShaderResourceBinding::sampledTexture(
            /*binding*/ 1,
            QRhiShaderResourceBinding::FragmentStage,
            m_sourceTexture,
            m_sourceSampler),

        QRhiShaderResourceBinding::sampledTexture(
            /*binding*/ 2,
            QRhiShaderResourceBinding::FragmentStage,
            m_maskTexture,
            m_maskSampler),
    });
    m_shaderResourceBindings->create();
}

void PixelationShader::bindToPipeline(QRhiGraphicsPipeline* pipeline)
{
    if (!NX_ASSERT(
        m_vertexShader.isValid() && m_fragmentShader.isValid() && m_shaderResourceBindings))
    {
        return;
    }

    pipeline->setShaderStages({
        {QRhiShaderStage::Vertex,  m_vertexShader},
        {QRhiShaderStage::Fragment, m_fragmentShader}
    });

    pipeline->setVertexInputLayout(m_inputLayout);
    pipeline->setShaderResourceBindings(m_shaderResourceBindings.get());
}

void PixelationShader::setupVertexInputLayout(
    uint32_t stride, uint32_t vertexOffset, QRhiVertexInputAttribute::Format vertexFormat)
{
    m_inputLayout.setBindings({
        QRhiVertexInputBinding{stride}
    });
    m_inputLayout.setAttributes({
        QRhiVertexInputAttribute{/*binding*/ 0, /*location*/ 0, vertexFormat, vertexOffset}
    });
}

} // nx::vms::common::pixelation
