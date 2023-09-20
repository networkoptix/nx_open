// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "graphics_web_view.h"

#include <QtCore/QMimeData>
#include <QtQuick/QQuickItem>
#include <QtWidgets/QGraphicsSceneDragDropEvent>

#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/common/widgets/webview_controller.h>
#include <nx/vms/client/desktop/resource_properties/camera/utils/camera_web_page_workarounds.h>
#include <nx/vms/client/desktop/style/webview_style.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>

namespace
{

enum ProgressValues
{
    kProgressValueFailed = -1,
    kProgressValueStarted = 0,
    kProgressValueLoading = 85, // Up to 85% value of progress
                                // we assume that page is in loading status
    kProgressValueLoaded = 100
};

WebViewPageStatus statusFromProgress(int progress)
{
    switch (progress)
    {
        case kProgressValueFailed:
            return kPageLoadFailed;
        case kProgressValueLoaded:
            return kPageLoaded;
        default:
            return (progress < kProgressValueLoading ? kPageInitialLoadInProgress : kPageLoading);
    }
}

} // namespace

namespace nx::vms::client::desktop {

GraphicsWebEngineView::GraphicsWebEngineView(const QnResourcePtr &resource, QGraphicsItem *parent):
    GraphicsQmlView(parent),
    m_controller(new WebViewController(this))
{
    setProperty(Qn::NoHandScrollOver, true);
    setAcceptDrops(true);

    m_controller->setMenuSave(true);

    m_controller->load(resource);

    connect(m_controller.data(), &WebViewController::urlChanged,
        this, &GraphicsWebEngineView::urlChanged);
    connect(m_controller.data(), &WebViewController::urlChanged,
        this, &GraphicsWebEngineView::updateCanGoBack);
    connect(m_controller.data(), &WebViewController::loadingStatusChanged,
        this, &GraphicsWebEngineView::setViewStatus);
    connect(m_controller.data(), &WebViewController::loadProgressChanged,
        this, &GraphicsWebEngineView::onLoadProgressChanged);

    m_controller->loadIn(this);

    const auto progressHandler = [this](int progress)
    {
        setStatus(statusFromProgress(progress));
    };

    const auto loadStartedHander = [this, progressHandler]()
    {
        connect(this, &GraphicsWebEngineView::loadProgress, this, progressHandler);
        setStatus(statusFromProgress(kProgressValueStarted));
    };

    const auto loadFinishedHandler = [this](bool successful)
    {
        disconnect(this, &GraphicsWebEngineView::loadProgress, this, nullptr);
        setStatus(statusFromProgress(successful ? kProgressValueLoaded : kProgressValueFailed));
    };

    connect(this, &GraphicsWebEngineView::loadStarted, this, loadStartedHander);
    connect(this, &GraphicsWebEngineView::loadFinished, this, loadFinishedHandler);
    connect(this, &GraphicsWebEngineView::loadProgress, this, progressHandler);
}

GraphicsWebEngineView::~GraphicsWebEngineView()
{
}

void GraphicsWebEngineView::onLoadProgressChanged()
{
    emit loadProgress(m_controller->loadProgress());
}

void GraphicsWebEngineView::setViewStatus(int status)
{
    // Sometimes canGoBack changes its value long after urlChanged is emitted.
    // Update its value when page loading status is modified.
    updateCanGoBack();

    switch (status)
    {
        case WebViewController::LoadStartedStatus:
            setStatus(kPageInitialLoadInProgress);
            emit loadStarted();
            break;
        case WebViewController::LoadStoppedStatus:
            setStatus(kPageLoaded);
            emit loadFinished(true);
            break;
        case WebViewController::LoadSucceededStatus:
            setStatus(kPageLoaded);
            emit loadFinished(true);
            break;
        case WebViewController::LoadFailedStatus:
        default:
            setStatus(kPageLoadFailed);
            emit loadFinished(false);
    }
}

void GraphicsWebEngineView::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
    if (const QMimeData* mimeData = event->mimeData();
        mimeData->hasText()
        || mimeData->hasHtml()
        || mimeData->hasUrls()
        || mimeData->hasImage()
        || mimeData->hasColor())
    {
        return GraphicsQmlView::dragEnterEvent(event);
    }

    event->ignore();
    return;
}

void GraphicsWebEngineView::updateCanGoBack()
{
    if (QQuickItem* webView = rootObject())
        setCanGoBack(m_controller->canGoBack());
}

WebViewPageStatus GraphicsWebEngineView::status() const
{
    return m_status;
}

bool GraphicsWebEngineView::canGoBack() const
{
    return m_canGoBack;
}

void GraphicsWebEngineView::setCanGoBack(bool value)
{
    if (m_canGoBack == value)
        return;

    m_canGoBack = value;
    emit canGoBackChanged();
}

void GraphicsWebEngineView::reload()
{
    m_controller->reload();
}

QUrl GraphicsWebEngineView::url() const
{
    return m_controller->url();
}

void GraphicsWebEngineView::back()
{
    m_controller->goBack();
}

void GraphicsWebEngineView::setStatus(WebViewPageStatus value)
{
    if (m_status == value)
        return;

    m_status = value;
    emit statusChanged();
}

} // namespace nx::vms::client::desktop
