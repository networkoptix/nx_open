// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>

#include "abstract_backend.h"

class QSettings;

namespace nx {
namespace utils {
namespace property_storage {

class NX_VMS_COMMON_API QSettingsBackend: public AbstractBackend
{
public:
    /**
     * Note: backend takes ownership of QSettings object.
     */
    QSettingsBackend(QSettings* settings, const QString& group = QString());
    virtual ~QSettingsBackend() override;

    virtual QString readValue(const QString& name, bool* success = nullptr) override;
    virtual bool writeValue(const QString& name, const QString& value) override;
    virtual bool removeValue(const QString& name) override;
    virtual bool sync() override;

private:
    QScopedPointer<QSettings> m_settings;
};

} // namespace property_storage
} // namespace utils
} // namespace nx
