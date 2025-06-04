// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_test_dialog.h"

#include <QtCore/QJsonObject>

#include <api/server_rest_connection.h>
#include <core/resource/media_server_resource.h>
#include <nx/utils/qobject.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/debug_utils/utils/debug_custom_actions.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/rules/basic_event.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/manifest.h>
#include <nx/vms/rules/utils/field_names.h>
#include <nx/vms/rules/utils/serialization.h>
#include <nx/vms/rules/utils/type.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop::rules {

namespace {

QSet<QByteArray> propertyNames(nx::vms::rules::BasicEvent* event)
{
    auto properties = nx::utils::propertyNames(event);
    properties.remove(vms::rules::utils::kType);
    properties.remove(vms::rules::utils::kTimestampFieldName);

    return properties;
}

} // namespace

EventTestDialog::EventTestDialog(QWidget* parent)
    : QmlDialogWrapper(
        appContext()->qmlEngine(),
        QUrl("Nx/VmsRules/EventTestDialog.qml"),
        /*initialProperties*/ {},
        parent),
    CurrentSystemContextAware{parent}
{
    QmlProperty<QObject*>(rootObjectHolder(), "dialog") = this;

    QVariantList events;
    const auto engine = systemContext()->vmsRulesEngine();
    for (const auto& [event, descriptor]: engine->events().asKeyValueRange())
        events.push_back(QVariantMap{{"type", event}, {"display", descriptor.displayName.value()}});

    QmlProperty<QVariantList>(rootObjectHolder(), "events") = events;
}

QVariantMap EventTestDialog::getEventProperties(const QString& eventType) const
{
    const auto event =
        systemContext()->vmsRulesEngine()->buildEvent({{nx::vms::rules::utils::kType, eventType}});

    QVariantMap result;
    const auto serializedProperties =
        vms::rules::serializeProperties(event.get(), propertyNames(event.get()));
    for (const auto& [k, v]: serializedProperties.asKeyValueRange())
        result.insert(k, v);

    if (auto serverIdIt = result.find(vms::rules::utils::kServerIdFieldName); serverIdIt != result.end())
        serverIdIt.value() = QJsonValue{currentServer()->getId().toSimpleString()};

    return result;
}

QVariantMap EventTestDialog::getEventPropertyMetatypes(const QString& eventType) const
{
    const auto event =
        systemContext()->vmsRulesEngine()->buildEvent({{nx::vms::rules::utils::kType, eventType}});

    QVariantMap result;
    const auto eventMetaObject = event->metaObject();
    for (const auto& propertyName: propertyNames(event.get()))
    {
        const auto property =
            eventMetaObject->property(eventMetaObject->indexOfProperty(propertyName));

        result.insert(propertyName, property.typeName());
    }

    return result;
}

void EventTestDialog::testEvent(const QString& event)
{
    const auto api = systemContext()->connectedServerApi();
    if (!NX_ASSERT(api))
        return;

    api->sendRequest<rest::ErrorOrData<QByteArray>>(
        /*sessionTokenHelper*/ {},
        network::http::Method::post,
        "/rest/v4/events/create",
        /*params*/ {},
        event,
        [this](bool success, rest::Handle /*requestId*/, rest::ErrorOrData<QByteArray> result)
        {
            if (success)
            {
                emit eventSent(success);
                return;
            }

            QString errorDescription;
            if (result.has_value())
            {
                errorDescription = result.value();
            }
            else
            {
                const auto& error = result.error();
                errorDescription = nx::format("Error id - %1").arg(error.errorId);
                if (!error.errorString.isEmpty())
                    errorDescription += nx::format(" , description - %2").arg(error.errorString);
            }

            emit eventSent(success, errorDescription);
        },
        this); //< TODO: #mmalofeev some custom header to be able find test events in the http log?
}

void EventTestDialog::registerAction()
{
    registerDebugAction(
        kDebugActionName,
        [](QnWorkbenchContext* context)
        {
            auto dialog = new EventTestDialog(context->mainWindowWidget());
            connect(dialog, &QmlDialogWrapper::done, dialog, &QObject::deleteLater);

            dialog->open();
        });
}

} // namespace nx::vms::client::desktop::rules
