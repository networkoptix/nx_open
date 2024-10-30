// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_property_backend.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>

#include <api/server_rest_connection.h>
#include <core/resource/user_resource.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/system_context.h>

namespace nx::vms::client::core::property_storage {

UserPropertyBackend::UserPropertyBackend(
    SystemContext* systemContext,
    const QString& propertyName,
    const SessionTokenHelperGetter& tokenHelperGetter)
    :
    SystemContextAware(systemContext),
    m_propertyName(propertyName),
    m_tokenHelperGetter(tokenHelperGetter)
{
    NX_CRITICAL(!propertyName.isEmpty());
}

bool UserPropertyBackend::isWritable() const
{
    return !systemContext()->user().isNull();
}

QString UserPropertyBackend::readValue(const QString& name, bool* success)
{
    const auto setSuccess =
        [&success](bool ok)
        {
            if (success)
                *success = ok;
        };

    if (m_deletedValues.contains(name))
    {
        setSuccess(false);
        return {};
    }

    if (m_newValues.contains(name))
    {
        setSuccess(true);
        return m_newValues.value(name);
    }

    const auto value = m_originalValues.value(name);
    switch (value.type())
    {
        case QJsonValue::String:
            setSuccess(true);
            return value.toString();

        case QJsonValue::Null:
        case QJsonValue::Undefined:
            break;

        default:
            NX_DEBUG(this, "Value %1 has non-string type %2", name, value.type());
            break;
    }

    setSuccess(false);
    return {};
}

bool UserPropertyBackend::writeValue(const QString& name, const QString& value)
{
    if (!systemContext()->user())
        return false;

    m_newValues[name] = value;
    m_deletedValues.remove(name);
    return true;
}

bool UserPropertyBackend::removeValue(const QString& name)
{
    if (!systemContext()->user())
        return false;

    m_newValues.remove(name);
    m_deletedValues.insert(name);
    return true;
}

bool UserPropertyBackend::exists(const QString& name) const
{
    return !m_deletedValues.contains(name)
        && (m_newValues.contains(name) || m_originalValues.contains(name));
}

bool UserPropertyBackend::sync()
{
    QnUserResourcePtr user = systemContext()->user();
    if (!user)
    {
        m_originalValues = {};
        m_newValues.clear();
        m_deletedValues.clear();

        NX_VERBOSE(this, "There is no current user. Nothing to sync.");
        return false;
    }

    NX_VERBOSE(this, "Synching settings...");

    m_originalValues = loadJsonFromProperty();

    const bool haveChanges = !m_deletedValues.isEmpty() || !m_newValues.isEmpty();

    if (!haveChanges)
        return true;

    for (const auto& [name, value]: m_newValues.asKeyValueRange())
        m_originalValues.insert(name, value);
    m_newValues.clear();

    for (const QString& name: m_deletedValues)
        m_originalValues.remove(name);
    m_deletedValues.clear();

    const api::ResourceWithParameters resourceWithParams{
        .parameters{{m_propertyName, m_originalValues}}};

    connectedServerApi()->patchRest(
        m_tokenHelperGetter(),
        "/rest/v4/users/" + user->getId().toString(),
        {},
        nx::reflect::json::serialize(resourceWithParams),
        [this](bool success, rest::Handle /*requestId*/, rest::ErrorOrData<QByteArray> /*reply*/)
        {
            NX_VERBOSE(this, "Settings save response received. Success: %1", success);
        });

    return true;
}

QJsonObject UserPropertyBackend::loadJsonFromProperty()
{
    const QString jsonString = systemContext()->user()->getProperty(m_propertyName);
    if (jsonString.isEmpty())
    {
        NX_VERBOSE(this, "Settings are empty.");
        return {};
    }

    QJsonParseError error;
    auto document = QJsonDocument::fromJson(jsonString.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError)
    {
        NX_ERROR(this, "Cannot parse property value: %1", error.errorString());
        return {};
    }

    if (!document.isObject())
    {
        NX_ERROR(this, "The resource property does not contain JSON object.");
        return {};
    }

    return document.object();
}

} // namespace nx::vms::client::core::property_storage
