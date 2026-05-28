// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

#include <core/resource/resource_fwd.h>
#include <nx/vms/api/analytics/descriptors.h>

namespace nx::analytics::taxonomy {

class NX_VMS_COMMON_API AbstractIntegration: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString id READ idAsQString CONSTANT)
    Q_PROPERTY(QString name READ nameAsQString CONSTANT)

public:
    AbstractIntegration(QObject* parent = nullptr):
        QObject(parent)
    {
    }

    virtual ~AbstractIntegration() {}

    virtual const std::string& id() const = 0;

    virtual const std::string& name() const = 0;

    virtual nx::vms::api::analytics::PluginDescriptor serialize() const = 0;

private:
    QString idAsQString() const;
    QString nameAsQString() const;
};

} // namespace nx::analytics::taxonomy
