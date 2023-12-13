// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rhi_offscreen_renderer.h"

#include <nx/pathkit/rhi_paint_device.h>

#include <QtGui/QImage>
#include <QtCore/QFile>

#if QT_VERSION >= QT_VERSION_CHECK(6,6,0)
    #include <rhi/qrhi.h>
#else
    #include <QtGui/private/qrhi_p.h>
#endif

namespace nx::pathkit {

RhiOffscreenRenderer::RhiOffscreenRenderer(QRhi* rhi, QSize size, QColor clearColor):
    m_size(size),
    m_rhi(rhi),
    m_clearColor(clearColor)
{
}

RhiOffscreenRenderer::~RhiOffscreenRenderer()
{}

QImage RhiOffscreenRenderer::render(RhiPaintDeviceRenderer* renderer)
{
    // Create render target
    std::unique_ptr<QRhiTexture> tex(m_rhi->newTexture(
        QRhiTexture::RGBA8,
        m_size,
        1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
    tex->create();
    std::unique_ptr<QRhiTextureRenderTarget> rt(m_rhi->newTextureRenderTarget({ tex.get() }));
    rp.reset(rt->newCompatibleRenderPassDescriptor());
    rt->setRenderPassDescriptor(rp.get());
    rt->create();

    // Render frame
    QRhiCommandBuffer* cb;

    m_rhi->beginOffscreenFrame(&cb);

    QRhiResourceUpdateBatch* u = m_rhi->nextResourceUpdateBatch();

    // Init renderer
    renderer->prepare(rp.get(), u);

    cb->beginPass(rt.get(), m_clearColor, {1.0f, 0}, u);
    cb->setViewport({0, 0, (float) m_size.width(), (float) m_size.height()});

    renderer->render(cb);

    QRhiReadbackResult readbackResult;

    u = m_rhi->nextResourceUpdateBatch();
    u->readBackTexture({tex.get()}, &readbackResult);
    cb->endPass(u);

    m_rhi->endOffscreenFrame();

    // Return result image.
    const QImage nonOwningImage(
        reinterpret_cast<const uchar*>(readbackResult.data.constData()),
        readbackResult.pixelSize.width(),
        readbackResult.pixelSize.height(),
        QImage::Format_RGBA8888_Premultiplied);

    if (m_rhi->isYUpInFramebuffer())
        return nonOwningImage.mirrored(); //< Mirror vertically.

    return nonOwningImage.copy();
}

} // namespace nx::pathkit
