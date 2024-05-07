// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "item_grabber.h"

#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>
#include <QtQml/QtQml>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtQuick/private/qquickitem_p.h>
#include <QtQuick/private/qquickpixmapcache_p.h>
#include <QtQuick/private/qsgadaptationlayer_p.h>
#include <QtQuick/private/qsgcontext_p.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/format.h>
#include <nx/vms/client/desktop/utils/qml_utils.h>
#include <utils/common/delayed.h>

namespace nx::vms::client::desktop {

namespace {

} // namespace

class ItemGrabberWorker: public QObject
{
public:
    ItemGrabberWorker(QQuickItem* item, const QJSValue& callback):
        m_item(item),
        m_window(item ? item->window() : nullptr),
        m_callback(callback),
        m_id(++m_idCounter)
    {
        if (!NX_ASSERT(m_item)
            || !NX_ASSERT(m_callback.isCallable())
            || !NX_ASSERT(m_window)
            || !QmlUtils::renderWindow(m_window)->isVisible())
        {
            m_item.clear(); //< To avoid erroneous dereferencing in the destructor.
            deleteLater();
            return;
        }

        connect(m_item.data(), &QObject::destroyed, this, &QObject::deleteLater);
        connect(m_window.data(), &QObject::destroyed, this, &QObject::deleteLater);

        connect(m_window.data(), &QQuickWindow::beforeSynchronizing,
            this, &ItemGrabberWorker::setup, Qt::DirectConnection);
        connect(m_window.data(), &QQuickWindow::afterRendering,
            this, &ItemGrabberWorker::render, Qt::DirectConnection);

        QQuickItemPrivate::get(m_item)->refFromEffectItem(false);
        m_window->update();
    }

    virtual ~ItemGrabberWorker() override
    {
        if (m_item)
            QQuickItemPrivate::get(m_item)->derefFromEffectItem(false);

        NX_ASSERT(!m_layer);
    }

private:
    void setup()
    {
        if (!m_item || !NX_ASSERT(m_window))
            return;

        const auto renderContext = QQuickWindowPrivate::get(m_window)->context;
        m_layer.reset(renderContext->sceneGraphContext()->createLayer(renderContext));
        m_layer->setItem(QQuickItemPrivate::get(m_item)->itemNode());
    }

    void render()
    {
        const auto layer = QScopedPointer<QSGLayer>(m_layer.take());
        if (!layer || !m_item || !NX_ASSERT(m_window))
            return;

        const auto itemSize = m_item->size();
        const auto devicePixelRatio = m_window->devicePixelRatio();
        const auto textureSize = (itemSize * devicePixelRatio).toSize();

        layer->setRect(QRectF(0, itemSize.height(), itemSize.width(), -itemSize.height()));
        const auto minimumSize = QQuickWindowPrivate::get(m_window)->context->sceneGraphContext()
            ->minimumFBOSize();

        layer->setDevicePixelRatio(devicePixelRatio);
        layer->setSize(textureSize.expandedTo(minimumSize));
        layer->scheduleUpdate();
        layer->updateTexture();

        auto image = layer->toImage();
        image.setDevicePixelRatio(devicePixelRatio);

        QUrl url;
        url.setScheme(QQuickPixmap::itemGrabberScheme);
        url.setPath(QVariant::fromValue(m_item.data()).toString());
        url.setFragment(QString::number(m_id));

        m_result.reset(new QQuickPixmap(url, image));
        executeLater([this]() { finish(); }, this);

        m_window->disconnect(this);
    }

    void finish()
    {
        // This object may be destroyed during the callback execution, so it should be called last.
        deleteLater();
        m_callback.call({ m_result->url().toString() });
    }

private:
    QPointer<QQuickItem> m_item;
    QPointer<QQuickWindow> m_window;
    QJSValue m_callback;
    QScopedPointer<QSGLayer> m_layer;
    QScopedPointer<QQuickPixmap> m_result;

    const quint64 m_id;
    static quint64 m_idCounter;
};

void ItemGrabber::grabToImage(QQuickItem* item, const QJSValue& callback)
{
    new ItemGrabberWorker(item, callback);
}

void ItemGrabber::registerQmlType()
{
    qmlRegisterSingletonType<ItemGrabber>("nx.vms.client.desktop", 1, 0, "ItemGrabber",
        [](QQmlEngine*, QJSEngine*) -> QObject* { return new ItemGrabber(); });
}

quint64 ItemGrabberWorker::m_idCounter = 0;

} // namespace nx::vms::client::desktop
