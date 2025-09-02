// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "scene_banners.h"
#include "private/scene_banners_p.h"

#include <nx/vms/client/core/utils/qml_helpers.h>

#include <utils/common/delayed.h>

namespace nx::vms::client::desktop {

namespace {

SceneBanners* g_instance = nullptr;

} // namespace


using namespace std::chrono;
using namespace nx::vms::client::core;

// ------------------------------------------------------------------------------------------------
// SceneBanners

SceneBanners::SceneBanners(QWidget* parentWidget):
    QObject(parentWidget),
    d(new Private(this, parentWidget))
{
    if (NX_ASSERT(!g_instance, "Singleton is created more than once"))
        g_instance = this;

    installEventHandler(d->container, QEvent::Resize, this,
        [this]()
        {
            const auto pos = (d->container->parentWidget()->size() - d->container->size()) / 2;
            d->container->move(pos.width(), pos.height());
        });

    if (d->container->status() == QQuickWidget::Ready)
        d->initialize();
    else
        connect(d->container, &QQuickWidget::statusChanged, this, [this]() { d->initialize(); });
}

SceneBanners::~SceneBanners()
{
    if (NX_ASSERT(g_instance == this))
        g_instance = nullptr;

    delete d->container; //< It may be already deleted.
}

SceneBanners* SceneBanners::instance()
{
    return g_instance;
}

nx::Uuid SceneBanners::add(const QString& text,
    std::optional<milliseconds> timeout,
    std::optional<QFont> font)
{
    if (!d->container || !d->container->rootObject())
        return {};

    return invokeQmlMethod<nx::Uuid>(d->container->rootObject(), "add", text,
        timeout.has_value() ? QVariant::fromValue(timeout->count()) : QVariant(),
        font.has_value() ? QVariant::fromValue(*font) : QVariant());
}

bool SceneBanners::remove(const nx::Uuid& id, bool immediately)
{
    return d->container && d->container->rootObject()
        ? invokeQmlMethod<bool>(d->container->rootObject(), "remove", id, immediately)
        : false;
}

bool SceneBanners::changeText(const nx::Uuid& id, const QString& newText)
{
    return d->container && d->container->rootObject()
        ? invokeQmlMethod<bool>(d->container->rootObject(), "changeText", id, newText)
        : false;
}

// ------------------------------------------------------------------------------------------------
// SceneBanner

SceneBanner::SceneBanner(const nx::Uuid& id, QObject* parent):
    QObject(parent),
    m_id(id)
{
    NX_CRITICAL(!m_id.isNull());

    connect(banners(), &SceneBanners::removed, this,
        [this](const nx::Uuid& removedId)
        {
            if (m_id != removedId)
                return;

            m_id = {};
            deleteLater();
        });
}

SceneBanners* SceneBanner::banners() const
{
    return static_cast<SceneBanners*>(parent());
}

SceneBanner::~SceneBanner()
{
    // We can get here in the QObject destructor of the SceneBanners object, on children deletion.
    if (auto sceneBanners = banners())
    {
        sceneBanners->disconnect(this);

        if (!m_id.isNull())
            sceneBanners->remove(m_id, /*immediately*/ true);
    }
}

SceneBanner* SceneBanner::show(const QString& text,
    std::optional<std::chrono::milliseconds> timeout,
    std::optional<QFont> font)
{
    if (auto id = SceneBanners::instance()->add(text, timeout, font); !id.isNull())
        return new SceneBanner(id, SceneBanners::instance());

    return nullptr;
}

bool SceneBanner::hide(bool immediately)
{
    return banners()->remove(m_id, immediately);
}

bool SceneBanner::changeText(const QString& value)
{
    return banners()->changeText(m_id, value);
}

} // namespace nx::vms::client::desktop
