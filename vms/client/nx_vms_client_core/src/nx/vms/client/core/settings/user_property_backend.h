// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QJsonObject>

#include <core/resource/resource_fwd.h>

#include <nx/utils/property_storage/abstract_backend.h>
#include <nx/vms/client/core/system_context_aware.h>
#include <nx/vms/utils/abstract_session_token_helper.h>

namespace nx::vms::client::core::property_storage {

/** Stores properties in a user resource's property as a map serialized to JSON. */
class NX_VMS_CLIENT_CORE_API UserPropertyBackend:
    public nx::utils::property_storage::AbstractBackend,
    public SystemContextAware
{
public:
    using SessionTokenHelperGetter = std::function<nx::vms::common::SessionTokenHelperPtr()>;

    UserPropertyBackend(
        SystemContext* systemContext,
        const QString& propertyName,
        const SessionTokenHelperGetter& tokenHelperGetter);

    virtual bool isWritable() const override;
    virtual QString readValue(const QString& name, bool* success = nullptr) override;
    virtual bool writeValue(const QString& name, const QString& value) override;
    virtual bool removeValue(const QString& name) override;
    virtual bool exists(const QString& name) const override;
    virtual bool sync() override;

private:
    QJsonObject loadJsonFromProperty();

protected:
    QString m_propertyName;
    SessionTokenHelperGetter m_tokenHelperGetter;
    QJsonObject m_originalValues;
    QHash<QString, QString> m_newValues;
    QSet<QString> m_deletedValues;
};

} // namespace nx::vms::client::core::property_storage
