// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/engine_manifest.h>

namespace nx::analytics::taxonomy {

class AbstractIntegration;

class NX_VMS_COMMON_API AbstractEngine: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)

public:
    AbstractEngine(QObject* parent = nullptr):
        QObject(parent)
    {
    }

    virtual  ~AbstractEngine() {};

    virtual QString id() const = 0;

    virtual QString name() const = 0;

    virtual AbstractIntegration* integration() const = 0;

    virtual nx::vms::api::analytics::EngineCapabilities capabilities() const = 0;

    virtual nx::vms::api::analytics::EngineDescriptor serialize() const = 0;
};

} // namespace nx::analytics::taxonomy
