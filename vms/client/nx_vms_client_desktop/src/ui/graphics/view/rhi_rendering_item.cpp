// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rhi_rendering_item.h"

#include <QtCore/QFile>
#include <QtCore/QRunnable>
#include <QtQuick/QQuickWindow>

#if QT_VERSION >= QT_VERSION_CHECK(6,6,0)
    #include <rhi/qrhi.h>
#else
    #include <QtGui/private/qrhi_p.h>
#endif

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log_main.h>

using namespace nx::pathkit;

namespace {

QRhi* getRhi(QQuickWindow* window)
{
    #if QT_VERSION >= QT_VERSION_CHECK(6,6,0)
        return window->rhi();
    #else
        const auto ri = window->rendererInterface();
        return static_cast<QRhi*>(ri->getResource(window, QSGRendererInterface::RhiResource));
    #endif
}

QRhiSwapChain* getSwapChain(QQuickWindow* window)
{
    #if QT_VERSION >= QT_VERSION_CHECK(6,6,0)
        return m_window->swapChain();
    #else
        QSGRendererInterface* rif = window->rendererInterface();
        return static_cast<QRhiSwapChain*>(
            rif->getResource(window, QSGRendererInterface::RhiSwapchainResource));
    #endif
}

} // namespace

RhiRenderingItem::RhiRenderingItem()
{
    connect(this, &QQuickItem::windowChanged, this, &RhiRenderingItem::handleWindowChanged);
}

RhiRenderingItem::~RhiRenderingItem()
{}

void RhiRenderingItem::handleWindowChanged(QQuickWindow* win)
{
    if (win)
    {
        connect(win, &QQuickWindow::beforeSynchronizing, this,
            &RhiRenderingItem::sync, Qt::DirectConnection);
        connect(win, &QQuickWindow::sceneGraphInvalidated, this,
            &RhiRenderingItem::cleanup, Qt::DirectConnection);
    }
}

// The safe way to release custom graphics resources is to both connect to
// sceneGraphInvalidated() and implement releaseResources(). To support
// threaded render loops the latter performs the RhiRenderingItem destruction
// via scheduleRenderJob(). Note that the RhiRenderingItem may be gone by the time
// the QRunnable is invoked.

void RhiRenderingItem::cleanup()
{
    // This function is invoked on the render thread, if there is one.

    delete m_renderer;
    m_renderer = nullptr;
}

class CleanupJob: public QRunnable
{
public:
    CleanupJob(RhiRenderingItemRenderer* renderer): m_renderer(renderer) {}
    void run() override { delete m_renderer; }
private:
    RhiRenderingItemRenderer* const m_renderer;
};

void RhiRenderingItem::releaseResources()
{
    if (m_renderer)
    {
        window()->scheduleRenderJob(
            new CleanupJob(m_renderer),
            QQuickWindow::BeforeSynchronizingStage);

        m_renderer = nullptr;
    }
}

void RhiRenderingItem::sync()
{
    // This function is invoked on the render thread, if there is one.

    if (!m_renderer)
    {
        m_renderer = new RhiRenderingItemRenderer;
        // Initializing resources is done before starting to record the
        // renderpass, regardless of wanting an underlay or overlay.
        connect(window(), &QQuickWindow::beforeRendering, m_renderer,
            &RhiRenderingItemRenderer::frameStart, Qt::DirectConnection);
        // Here we want an underlay and therefore connect to
        // beforeRenderPassRecording. Changing to afterRenderPassRecording
        // would render the RhiRenderingItem on top (overlay).
        connect(window(), &QQuickWindow::beforeRenderPassRecording, m_renderer,
            &RhiRenderingItemRenderer::mainPassRecordingStart, Qt::DirectConnection);
    }

    m_renderer->setWindow(window());
    m_renderer->syncPaintDevice(&m_pd);
}

RhiRenderingItemRenderer::RhiRenderingItemRenderer() {}

RhiRenderingItemRenderer::~RhiRenderingItemRenderer() {}

void RhiRenderingItemRenderer::setWindow(QQuickWindow* window)
{
    if (m_window == window)
        return;

    m_window = window;
    m_paintRenderer.reset(new RhiPaintDeviceRenderer(getRhi(m_window)));
}

void RhiRenderingItemRenderer::syncPaintDevice(RhiPaintDevice* pd)
{
    m_paintRenderer->sync(pd);
}

void RhiRenderingItemRenderer::frameStart()
{
    // This function is invoked on the render thread, if there is one.

    QRhi* rhi = getRhi(m_window);
    if (!NX_ASSERT(rhi, "QQuickWindow is not using QRhi for rendering"))
        return;

    if (rhi->isDeviceLost())
    {
        NX_WARNING(this, "RHI device lost at QQuickWindow::beforeRendering");
        return;
    }

    QRhiSwapChain* swapChain = getSwapChain(m_window);

    QSGRendererInterface* rif = m_window->rendererInterface();
    QRhiCommandBuffer* cb = swapChain
        ? swapChain->currentFrameCommandBuffer()
        : static_cast<QRhiCommandBuffer*>(
            rif->getResource(m_window, QSGRendererInterface::RhiRedirectCommandBuffer));

    QRhiRenderTarget* rt = swapChain
        ? swapChain->currentFrameRenderTarget()
        : static_cast<QRhiRenderTarget*>(
            rif->getResource(m_window, QSGRendererInterface::RhiRedirectRenderTarget));

    if (!NX_ASSERT(rt && cb, "No render target or no command buffer"))
        return;

    QRhiResourceUpdateBatch *resourceUpdates = rhi->nextResourceUpdateBatch();

    m_paintRenderer->prepare(rt->renderPassDescriptor(), resourceUpdates);
    cb->resourceUpdate(resourceUpdates);
}

void RhiRenderingItemRenderer::mainPassRecordingStart()
{
    // This function is invoked on the render thread, if there is one.

    QRhi* rhi = getRhi(m_window);
    if (!rhi)
        return;

    if (rhi->isDeviceLost())
    {
        NX_WARNING(this, "RHI device lost at QQuickWindow::beforeRenderPassRecording");
        return;
    }

    QRhiSwapChain* swapChain = getSwapChain(m_window);

    QSGRendererInterface* rif = m_window->rendererInterface();
    QRhiCommandBuffer* cb = swapChain
        ? swapChain->currentFrameCommandBuffer()
        : static_cast<QRhiCommandBuffer*>(
            rif->getResource(m_window, QSGRendererInterface::RhiRedirectCommandBuffer));

    QRhiRenderTarget* rt = swapChain
        ? swapChain->currentFrameRenderTarget()
        : static_cast<QRhiRenderTarget*>(
            rif->getResource(m_window, QSGRendererInterface::RhiRedirectRenderTarget));

    if (!cb || !rt)
        return;

    m_paintRenderer->render(cb);
}
