// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <core/resource/resource.h>
#include <ui/graphics/items/standard/graphics_qml_view.h>

namespace nx::vms::client::desktop { class WebViewController; }

enum WebViewPageStatus
{
    kPageInitialLoadInProgress
    , kPageLoading
    , kPageLoaded
    , kPageLoadFailed
};

namespace nx::vms::client::desktop {

class GraphicsWebEngineView: public GraphicsQmlView
{
    Q_OBJECT

    Q_PROPERTY(WebViewPageStatus status READ status WRITE setStatus NOTIFY statusChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack WRITE setCanGoBack NOTIFY canGoBackChanged)

public:
    static const QUrl kQmlSourceUrl;

    typedef std::function<void(void)> RootReadyCallback;

    GraphicsWebEngineView(const QnResourcePtr &resource, QGraphicsItem* parent = nullptr);

    virtual ~GraphicsWebEngineView();

    WebViewPageStatus status() const;

    void setStatus(WebViewPageStatus value);

    bool canGoBack() const;

    void setCanGoBack(bool value);

    QUrl url() const;

    WebViewController* controller() const { return m_controller.data(); }

public slots:
    void back();

    void reload();

signals:
    void loadStarted();

    void statusChanged();

    void canGoBackChanged();

    void loadProgress(int progress);

    void loadFinished(bool ok);

protected:
    virtual void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;

private slots:
    void updateCanGoBack();

    void setViewStatus(int status);

    void onLoadProgressChanged();

private:
    WebViewPageStatus m_status = kPageInitialLoadInProgress;
    bool m_canGoBack = false;
    QScopedPointer<WebViewController> m_controller;
};

} // namespace nx::vms::client::desktop
