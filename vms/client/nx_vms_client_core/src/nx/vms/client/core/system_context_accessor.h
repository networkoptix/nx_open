// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/resource/resource_fwd.h>

Q_MOC_INCLUDE("core/resource/resource.h")
Q_MOC_INCLUDE("nx/vms/client/core/system_context.h")

class QModelIndex;
class QnResourcePool;

namespace nx::vms::client::core {

class SystemContext;

/**
 * Helper class to access system context by the specified resource. Can be used in QML and as
 * a parent in C++ resource-related classes for convenient system context access.
 */
class NX_VMS_CLIENT_CORE_API SystemContextAccessor: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(QnResource* resource
        READ rawResource
        WRITE setRawResource
        NOTIFY resourceChanged)

    Q_PROPERTY(SystemContext* systemContext
        READ systemContext
        NOTIFY systemContextChanged)

public:
    static void registerQmlType();

    SystemContextAccessor(QObject* parent = nullptr);
    SystemContextAccessor(const QnResource* resource,
        QObject* parent = nullptr);
    SystemContextAccessor(const QnResourcePtr resource,
        QObject* parent = nullptr);
    SystemContextAccessor(const QModelIndex& index,
        QObject* parent = nullptr);

    virtual ~SystemContextAccessor() override;

    QnResource* rawResource() const;
    void setRawResource(const QnResource* value);

    SystemContext* systemContext() const;

    QnResourcePool* resourcePool() const;

signals:
    void resourceChanged();
    void systemContextIsAboutToBeChanged();
    void systemContextChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
