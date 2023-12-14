// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>

#include <nx/pathkit/rhi_paint_device.h>
#include <nx/pathkit/rhi_render.h>

class RhiRenderingItemRenderer: public QObject
{
    using base_type = QObject;
    Q_OBJECT

public:
    RhiRenderingItemRenderer();
    virtual ~RhiRenderingItemRenderer() override;

    void setWindow(QQuickWindow* window);
    void syncPaintDevice(nx::pathkit::RhiPaintDevice* pd);

public slots:
    void frameStart();
    void mainPassRecordingStart();

private:
    QQuickWindow* m_window = nullptr;

    std::unique_ptr<nx::pathkit::RhiPaintDeviceRenderer> m_paintRenderer;
};

/**
 * QML item for rendering data painted on nx::pathkit::RhiPaintDevice.
 * Rendering is perfromed under all QML elements in the QML window.
 * See https://doc.qt.io/qt-6/qtquick-scenegraph-rhiunderqml-example.html
 */
class RhiRenderingItem: public QQuickItem
{
    Q_OBJECT

public:
    RhiRenderingItem();
    virtual ~RhiRenderingItem() override;

    nx::pathkit::RhiPaintDevice* paintDevice() { return &m_pd; }

public slots:
    void sync();
    void cleanup();

private slots:
    void handleWindowChanged(QQuickWindow* win);

private:
    void releaseResources() override;

    nx::pathkit::RhiPaintDevice m_pd;

    RhiRenderingItemRenderer* m_renderer = nullptr;
};
