// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "call_alarm_manager.h"

#include <QtCore/QSet>
#include <QtQml/QtQml>

#include <nx/utils/log/assert.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/scoped_connections.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/handlers/notification_action_handler.h>
#include <nx/vms/event/actions/abstract_action.h>

namespace nx::vms::client::desktop {

using namespace nx::vms::common;

struct CallAlarmManager::Private
{
    CallAlarmManager* const q;
    WindowContext* context = nullptr;
    nx::utils::ScopedConnections connections;
    bool active = false;
    QSet<nx::Uuid> callingIntercomIds;

    auto makeAlarmGuard()
    {
        return nx::utils::makeScopeGuard(
            [this, wasRinging = q->isRinging()]()
            {
                if (q->isRinging() != wasRinging)
                    emit q->ringingChanged();
            });
    }

    void setContext(WindowContext* value)
    {
        if (context == value)
            return;

        const auto guard = makeAlarmGuard();
        callingIntercomIds.clear();

        connections.reset();
        context = value;
        emit q->contextChanged();

        if (!context)
            return;

        const auto handler = context->notificationActionHandler();
        if (!NX_ASSERT(handler))
            return;

        connections << connect(handler, &NotificationActionHandler::systemHealthEventAdded, q,
            [this](system_health::MessageType message, const QVariant& params)
            {
                handleEventAddedOrRemoved(message, params, /*added*/ true);
            });

        connections << connect(handler, &NotificationActionHandler::systemHealthEventRemoved, q,
            [this](system_health::MessageType message, const QVariant& params)
            {
                handleEventAddedOrRemoved(message, params, /*added*/ false);
            });
    }

    void setActive(bool value)
    {
        if (active == value)
            return;

        const auto guard = makeAlarmGuard();
        callingIntercomIds.clear();

        active = value;
        emit q->activeChanged();
    }

    void handleEventAddedOrRemoved(
        system_health::MessageType message, const QVariant& params, bool added)
    {
        if (!active || message != system_health::MessageType::showIntercomInformer)
            return;

        const auto action = params.value<nx::vms::event::AbstractActionPtr>();
        if (!action)
            return;

        const auto& runtimeParameters = action->getRuntimeParams();
        const nx::Uuid intercomId = runtimeParameters.eventResourceId;

        NX_ASSERT(!intercomId.isNull());

        const auto guard = makeAlarmGuard();
        if (added)
            callingIntercomIds.insert(intercomId);
        else
            callingIntercomIds.remove(intercomId);
    }
};

CallAlarmManager::CallAlarmManager(QObject* parent):
    QObject(parent),
    d(new Private{.q = this})
{
}

CallAlarmManager::CallAlarmManager(WindowContext* context, QObject* parent):
    CallAlarmManager(parent)
{
    setContext(context);
}

CallAlarmManager::~CallAlarmManager()
{
    // Required here for forward-declared scoped pointer destruction.
}

WindowContext* CallAlarmManager::context() const
{
    return d->context;
}

void CallAlarmManager::setContext(WindowContext* value)
{
    d->setContext(value);
}

bool CallAlarmManager::isRinging() const
{
    return d->active && !d->callingIntercomIds.empty();
}

bool CallAlarmManager::isActive() const
{
    return d->active;
}

void CallAlarmManager::setActive(bool value)
{
    d->setActive(value);
}

void CallAlarmManager::registerQmlType()
{
    qmlRegisterType<CallAlarmManager>("nx.vms.client.desktop", 1, 0, "CallAlarmManager");
}

} // namespace nx::vms::client::desktop
