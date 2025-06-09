// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_test_dialog.h"

#include <QtCore/QJsonObject>

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
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

QStringList getAvailableStates(const vms::rules::ItemDescriptor& descriptor)
{
    QStringList availableStates;
    if (descriptor.flags.testFlag(vms::rules::ItemFlag::instant))
        availableStates.push_back(toString(vms::rules::State::instant));

    if (descriptor.flags.testFlag(vms::rules::ItemFlag::prolonged))
    {
        availableStates.push_back(toString(vms::rules::State::started));
        availableStates.push_back(toString(vms::rules::State::stopped));
    }

    return availableStates;
}

// Returns simplified data types.
QString type(const QMetaType& metaType, const QString& name)
{
    const auto metaTypeId = metaType.id();
    if (qMetaTypeId<nx::vms::api::rules::State>() == metaTypeId)
        return "state";

    if (qMetaTypeId<nx::vms::api::EventReason>() == metaTypeId)
        return "eventReason";

    if (qMetaTypeId<nx::vms::api::EventLevel>() == metaTypeId)
        return "eventLevel";

    if (qMetaTypeId<bool>() == metaTypeId)
        return "bool";

    if (qMetaTypeId<nx::Uuid>() == metaTypeId)
    {
        if (name == vms::rules::utils::kServerIdFieldName)
            return "server";

        if (name == vms::rules::utils::kDeviceIdFieldName)
            return "device";

    }

    return "string";
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

    QStringList eventReasons;
    for (auto reason: reflect::enumeration::allEnumValues<nx::vms::api::EventReason>())
        eventReasons.push_back(toString(reason));

    QmlProperty<QStringList>(rootObjectHolder(), "eventReasons") = eventReasons;

    QStringList eventLevels;
    for (auto level: reflect::enumeration::allEnumValues<nx::vms::api::EventLevel>())
        eventLevels.push_back(toString(level));

    QmlProperty<QStringList>(rootObjectHolder(), "eventLevels") = eventLevels;

    QVariantList servers;
    for (const auto& server: systemContext()->resourcePool()->servers())
    {
        servers.push_back(
            QVariantMap{{"name", server->getName()}, {"value", server->getId().toSimpleString()}});
    }
    QmlProperty<QVariantList>(rootObjectHolder(), "servers") = servers;

    QVariantList devices;
    for (const auto& device: systemContext()->resourcePool()->getAllCameras())
    {
        devices.push_back(
            QVariantMap{
                {"name", device->getName()},
                {"value", device->getId().toSimpleString()},
                {"parent", device->getParentId().toSimpleString()}});
    }
    QmlProperty<QVariantList>(rootObjectHolder(), "devices") = devices;
}

void EventTestDialog::onEventSelected(const QString& eventType)
{
    const auto engine = systemContext()->vmsRulesEngine();
    const auto descriptor = engine->eventDescriptor(eventType);
    const auto event = engine->buildEvent({{nx::vms::rules::utils::kType, eventType}});
    const auto properties = propertyNames(event.get());
    const auto eventMetaObject = event->metaObject();

    auto serializedProperties =
        vms::rules::serializeProperties(event.get(), properties);

    const auto availableStates = getAvailableStates(descriptor.value());
    QmlProperty<QStringList>(rootObjectHolder(), "availableStates") = availableStates;

    if (auto stateIt = serializedProperties.find(vms::rules::utils::kStateFieldName);
        stateIt != serializedProperties.end())
    {
        *stateIt = QJsonValue{availableStates.first()};
    }

    if (auto serverIdIt = serializedProperties.find(vms::rules::utils::kServerIdFieldName);
        serverIdIt != serializedProperties.end())
    {
        *serverIdIt = QJsonValue{currentServer()->getId().toSimpleString()};
    }

    QVariantList propertiesModel;
    for (const auto& propertyName: properties)
    {
        propertiesModel.push_back(QVariantMap{
            {"name", propertyName},
            {"value", serializedProperties.value(propertyName).toJson(QJsonDocument::Compact)},
            {"type", type(eventMetaObject->property(eventMetaObject->indexOfProperty(propertyName)).metaType(), propertyName)}});
    }

    QmlProperty<QVariantList>(rootObjectHolder(), "properties") = propertiesModel;
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
                    errorDescription += nx::format(": %2").arg(error.errorString);
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
