// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "interactive_item_registry.h"

#include <nx/ranges.h>
#include <nx/utils/qobject.h>
#include <nx/vms/client/core/window_context_qml_initializer.h>
#include <nx/vms/client/mobile/window_context.h>

namespace nx::vms::client::mobile {

namespace {

std::optional<QMetaMethod> findInteractionSignal(QObject* object)
{
    static constexpr QByteArrayView kDefaultInteractionSignals[] = {"clicked", "tapped"};

    for (const auto& signalName: kDefaultInteractionSignals)
    {
        if (const auto signal = nx::utils::findSignal(object, signalName))
            return signal;
    }

    return std::nullopt;
}

} // namespace

InteractiveItemAttached::InteractiveItemAttached(
    InteractiveItemRegistry* registry, QQuickItem* parent):
    QObject(parent),
    m_item(parent),
    m_registry(registry),
    m_interactionSource(m_item),
    m_interactive(m_item, "interactive"),
    m_update([this]() { updateAvailability(); }, std::chrono::milliseconds{0})
{
    m_update.setFlags(utils::PendingOperation::NoFlags);

    connect(m_item, &QQuickItem::visibleChanged, this, &InteractiveItemAttached::requestUpdate);
    connect(m_item, &QQuickItem::enabledChanged, this, &InteractiveItemAttached::requestUpdate);

    if (m_interactive.hasNotifySignal())
        m_interactive.connectNotifySignal(this, SLOT(requestUpdate()));

    requestUpdate();
    updateInteractionConnection();
}

InteractiveItemAttached::~InteractiveItemAttached()
{
    if (!NX_ASSERT(m_registry))
        return;

    if (!m_name.isEmpty())
        m_registry->remove(m_name, this);
}

void InteractiveItemAttached::setName(const QString& name)
{
    if (!NX_ASSERT(m_registry))
        return;

    if (m_name == name)
        return;

    if (!m_name.isEmpty())
        m_registry->remove(m_name, this);

    m_name = name;
    emit nameChanged();

    if (!m_name.isEmpty())
        m_registry->add(m_name, this);
}

void InteractiveItemAttached::setInteractionSource(QObject* source)
{
    if (m_interactionSource == source)
        return;

    m_interactionSource = source;
    emit interactionSourceChanged();

    updateInteractionConnection();
}

void InteractiveItemAttached::requestUpdate()
{
    m_update.requestOperation();
}

void InteractiveItemAttached::updateAvailability()
{
    const bool available = m_item->isVisible() && m_item->isEnabled()
        && (!m_interactive.isValid() || m_interactive.read().toBool());

    if (m_available == available)
        return;

    m_available = available;
    emit availableChanged();
}

void InteractiveItemAttached::updateInteractionConnection()
{
    const auto source = m_interactionSource ? m_interactionSource : m_item;

    m_interactionConnection.reset();

    if (const auto signal = findInteractionSignal(source))
    {
        m_interactionConnection.reset(connect(
            source, *signal, this, QMetaMethod::fromSignal(&InteractiveItemAttached::interacted)));
    }
}

InteractiveItemRegistry::InteractiveItemRegistry(QObject* parent): QObject(parent)
{
}

InteractiveItemRegistry::~InteractiveItemRegistry()
{
    NX_ASSERT(m_items.empty());
}

InteractiveItemAttached* InteractiveItemRegistry::qmlAttachedProperties(QObject* object)
{
    const auto item = qobject_cast<QQuickItem*>(object);
    if (!NX_ASSERT(item, "InteractiveItem can only be attached to an Item-derived object."))
        return nullptr;

    const auto coreContext = WindowContext::fromQmlContext(object)->context();
    if (!NX_ASSERT(coreContext))
        return nullptr;

    const auto context = coreContext->as<WindowContext>();
    if (!NX_ASSERT(context))
        return nullptr;

    return new InteractiveItemAttached(context->interactiveItemRegistry(), item);
}

void InteractiveItemRegistry::registerQmlType()
{
    qmlRegisterUncreatableType<InteractiveItemRegistry>("nx.vms.client.mobile",
        1,
        0,
        "InteractiveItem",
        "InteractiveItem is not a creatable type");
}

void InteractiveItemRegistry::add(const QString& name, InteractiveItemAttached* item)
{
    m_items.insert(name, item);
    emit itemAdded(name, item);
}

void InteractiveItemRegistry::remove(const QString& name, InteractiveItemAttached* item)
{
    m_items.remove(name, item);
    emit itemRemoved(name, item);
}

QList<InteractiveItemAttached*> InteractiveItemRegistry::attachedItems(const QString& name) const
{
    return m_items.values(name);
}

QList<QQuickItem*> InteractiveItemRegistry::items(const QString& name) const
{
    return attachedItems(name)
        | std::views::transform([](InteractiveItemAttached* item) { return item->item(); })
        | nx::ranges::to<QList>();
}

} // namespace nx::vms::client::mobile
