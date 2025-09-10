// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/property_storage/filesystem_backend.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

/**
 * Stores properties in separate files in the Local Application Data directory, in the subfolder
 * corresponding to the System ID.
 */
class SystemSpecificFileSystemBackend:
    public QObject,
    public nx::utils::property_storage::FileSystemBackend,
    public SystemContextAware
{
    using base_type = nx::utils::property_storage::FileSystemBackend;

public:
    SystemSpecificFileSystemBackend(SystemContext* systemContext, QObject* parent = nullptr);

    virtual bool isWritable() const override;
    virtual QString readValue(const QString& name, bool* success = nullptr) override;
    virtual bool writeValue(const QString& name, const QString& value) override;
    virtual bool removeValue(const QString& name) override;
    virtual bool exists(const QString& name) const override;
    virtual bool sync() override;
    virtual bool writeDocumentation(const QString& docText) override;

    static QStringList systemSpecificStoragePaths();

private:
    /** System-specific settings shall not be written when the user is logged out. */
    bool m_valid = false;
};

} // namespace nx::vms::client::desktop
