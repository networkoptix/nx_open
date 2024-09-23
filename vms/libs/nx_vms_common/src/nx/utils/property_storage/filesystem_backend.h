// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QDir>

#include "abstract_backend.h"

#include <nx/utils/impl_ptr.h>

namespace nx::utils::property_storage {

/** Stores properties in separate files in the given directory. */
class NX_VMS_COMMON_API FileSystemBackend: public AbstractBackend
{
public:
    FileSystemBackend(const QDir& path);

    virtual QString readValue(const QString& name, bool* success = nullptr) override;
    virtual bool writeValue(const QString& name, const QString& value) override;
    virtual bool removeValue(const QString& name) override;
    virtual bool sync() override;

protected:
    QDir m_path;
};

} // namespace nx::utils::property_storage
